#include "Ability/Inventory/InventoryModule.h"
#include "Common/LifecycleTypes.h"
#include "Ability/Inventory/InventoryItemTypes.h"
#include "Ability/Inventory/InventorySnapshotTypes.h"
#include "Ability/Inventory/InventoryCommandTypes.h"

#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/InventoryCommitCoordinator.h"
#include "Ability/Inventory/InventoryItemAssetCatalog.h"
#include "Ability/Inventory/InventorySnapshotCodec.h"
#include "Ability/Inventory/InventoryTransactionService.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "Misc/Crc.h"
#include "Main/MainModule.h"

UInventoryModule::UInventoryModule(FVTableHelper& Helper) : Super(Helper)
{
}

UInventoryModule::UInventoryModule()
{
    ModuleName = TEXT("InventoryModule");
    bModuleRequired = true;
    ArchiveFreeze = MakeShared<FInventoryArchiveFreeze, ESPMode::ThreadSafe>();
}

UInventoryModule::~UInventoryModule()
{
}

UInventoryModule* UInventoryModule::GetPtr(UWorld* InWorld)
{
    AMainModule* Owner = AMainModule::GetPtr(InWorld);
    if (!Owner || !Owner->CanResolveModules())
    {
        return nullptr;
    }
    UModuleBase* const* Module = Owner->GetModuleMap().Find(GetDefault<UInventoryModule>()->GetModuleName());
    UInventoryModule* Inventory = Module ? Cast<UInventoryModule>(*Module) : nullptr;
    return Inventory && Inventory->IsBusinessAdmissionOpen() ? Inventory : nullptr;
}

void UInventoryModule::OnTermination(EPhase InPhase)
{
    if (InPhase == EPhase::Final)
    {
        bClosing = true;
        FString Error;
        if (!DrainQueuedRequestsForClose(Error) || !PrepareInventoryTermination(Error) ||
            !CloseStore(!Journal.IsTransient(), Error))
        {
            UE_LOG(LogTemp, Error, TEXT("库存关闭失败：%s"), *Error);
        }
        else
        {
            Adapters.Reset();
        }
    }
    Super::OnTermination(InPhase);
}

bool UInventoryModule::PrepareInventoryTermination(FString& OutError)
{
    return true;
}

void UInventoryModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
    Super::OnRefresh(DeltaSeconds, bInEditor);
    if(bInEditor || !IsReady() || bCommitting || bPublishing || bDrainingQueue || IsArchiveFrozen())
    {
        return;
    }
    if (PendingRetire.IsEmpty() && Queue.IsEmpty()) return;
    TGuardValue<bool> Draining(bDrainingQueue, true);
    const TArray<FGuid> Retiring = PendingRetire.Array();
    for(const FGuid& InventoryId : Retiring)
    {
        PendingRetire.Remove(InventoryId);
        FString Error;
        if(!LeaseCounts.Contains(InventoryId) && !Tombstones.Contains(InventoryId))
        {
            if (Retire(InventoryId, Error))
            {
                FailedRetireReports.Remove(InventoryId);
            }
            else
            {
                PendingRetire.Add(InventoryId);
                if (!FailedRetireReports.Contains(InventoryId))
                {
                    FailedRetireReports.Add(InventoryId);
                    UE_LOG(LogTemp, Error, TEXT("库存退役失败：%s %s"), *InventoryId.ToString(), *Error);
                }
            }
        }
    }
    if(Queue.IsEmpty())
    {
        return;
    }
    const int32 Count = FMath::Min(Queue.Num(), 32);
    for(int32 RequestIndex = 0; RequestIndex < Count && !Queue.IsEmpty(); ++RequestIndex)
    {
        if (IsArchiveFrozen()) break;
        FQueuedRequest Entry = MoveTemp(Queue[0]);
        Queue.RemoveAt(0, 1, EAllowShrinking::No);
        const FInventoryCommandResult Result = ExecuteAuthorizedBatch(Entry.Request, Entry.Authority);
        if(Entry.Completion)
        {
            Entry.Completion(Result);
        }
    }
}

bool UInventoryModule::Configure(TConstArrayView<FInventoryItemDefinition> InItems, TConstArrayView<FInventoryProfileDefinition> InProfiles,
    TSharedPtr<FInventoryRuleProvider> InRules, FString& OutError)
{
    if(!IsBusinessAdmissionOpen() || !IsInGameThread() || bOpen || bConfigured || InProfiles.IsEmpty())
    {
        OutError = TEXT("Inventory.ConfigurationInvalid");
        return false;
    }
    FInventoryItemRegistry NewRegistry;
    if(!NewRegistry.Initialize(InItems, OutError))
    {
        return false;
    }
    TMap<FPrimaryAssetId, FInventoryProfileDefinition> NewProfiles;
    for(const FInventoryProfileDefinition& Profile : InProfiles)
    {
        if(NewProfiles.Contains(Profile.ProfileId) || !Profile.ValidateDefinition(OutError))
        {
            if(OutError.IsEmpty())
            {
                OutError = TEXT("Inventory.DuplicateProfile");
            }
            return false;
        }
        NewProfiles.Add(Profile.ProfileId, Profile);
    }
    Registry = MoveTemp(NewRegistry);
    Profiles = MoveTemp(NewProfiles);
    Rules = MoveTemp(InRules);
    bConfigured = true;
    return true;
}

bool UInventoryModule::SetItemAssets(UInventoryItemAssetCatalog* InAssets)
{
    if(!IsBusinessAdmissionOpen() || bOpen || !IsInGameThread() || !::IsValid(InAssets))
    {
        return false;
    }
    ItemAssets = InAssets;
    return true;
}

const UInventoryItemAssetCatalog* UInventoryModule::GetItemAssets() const
{
    return ItemAssets;
}

bool UInventoryModule::RegisterRecordDomain(FName InDomain, int32 InMaximumBytes, FInventoryRecordValidator InValidator, FString& OutError)
{
    if(!IsBusinessAdmissionOpen() || bOpen || !IsInGameThread())
    {
        OutError = TEXT("Inventory.DomainRegistrationClosed");
        return false;
    }
    return Records.RegisterDomain(InDomain, InMaximumBytes, MoveTemp(InValidator), OutError);
}

bool UInventoryModule::RegisterDomainAdapter(TSharedRef<IInventoryDomainAdapter> InAdapter, FString& OutError)
{
    if(!IsBusinessAdmissionOpen() || bOpen || !IsInGameThread() || InAdapter->GetDomain().IsNone() || Adapters.Contains(InAdapter->GetDomain()))
    {
        OutError = TEXT("Inventory.DomainAdapterInvalid");
        return false;
    }
    Adapters.Add(InAdapter->GetDomain(), InAdapter);
    return true;
}

bool UInventoryModule::RegisterSnapshotProjector(FName InName, FInventorySnapshotProjector InProjector, FString& OutError)
{
    if(!IsBusinessAdmissionOpen() || bOpen || !IsInGameThread() || InName.IsNone() || !InProjector || SnapshotProjectors.Contains(InName))
    {
        OutError = TEXT("Inventory.SnapshotProjectorInvalid");
        return false;
    }
    SnapshotProjectors.Add(InName, MoveTemp(InProjector));
    return true;
}

bool UInventoryModule::OpenStore(const FGuid& InWorldId, const FGuid& InStoreId, const FString& InDirectory, bool bInCreateNew, FString& OutError)
{
    return OpenStoreInternal(InWorldId, InStoreId, InDirectory, bInCreateNew, false, OutError);
}

bool UInventoryModule::OpenTransientStore(const FGuid& InWorldId, const FGuid& InStoreId, FString& OutError)
{
    return OpenStoreInternal(InWorldId, InStoreId, FString(), true, true, OutError);
}

bool UInventoryModule::IsTransientStore() const
{
    return bOpen && Journal.IsTransient();
}

bool UInventoryModule::OpenStoreInternal(const FGuid& InWorldId, const FGuid& InStoreId, const FString& InDirectory,
    bool bInCreateNew, bool bInTransient, FString& OutError)
{
    if(!IsBusinessAdmissionOpen() || !IsInGameThread() || !bConfigured || bOpen || !GetWorld() || GetWorld()->GetNetMode() == NM_Client)
    {
        OutError = TEXT("Inventory.OpenStoreInvalid");
        return false;
    }
    if(StoreSessionHistory == EStoreSessionHistory::TransientOpened ||
        (bInTransient && StoreSessionHistory != EStoreSessionHistory::None))
    {
        OutError = TEXT("Inventory.TransientSessionOneShot");
        return false;
    }
    bQuarantined = false;
    bClosing = false;
    bCloseNotificationSent = false;
    FailureReason.Reset();
    FInventoryCheckpoint Checkpoint;
    TArray<FInventoryJournalRecord> JournalRecords;
    const bool bJournalOpened = bInTransient
        ? Journal.OpenTransient(InWorldId, InStoreId, Checkpoint, JournalRecords, OutError)
        : Journal.Open(InDirectory, InWorldId, InStoreId, bInCreateNew, Checkpoint, JournalRecords, OutError);
    if(!bJournalOpened)
    {
        Quarantine(OutError);
        return false;
    }
    StoreSessionHistory = bInTransient ? EStoreSessionHistory::TransientOpened : EStoreSessionHistory::PersistentOpened;
    WorldId = InWorldId;
    StoreId = InStoreId;
    WorldEpoch = FGuid::NewGuid();
    Cold = MoveTemp(Checkpoint.Inventories);
    Tombstones = MoveTemp(Checkpoint.Tombstones);
    if(!Records.Restore(Checkpoint.Domains, OutError))
    {
        Quarantine(OutError);
        return false;
    }
    FInventoryCommitCoordinator Coordinator(*this);
    for(const FInventoryJournalRecord& Record : JournalRecords)
    {
        if(!Coordinator.Replay(Record, OutError))
        {
            Quarantine(OutError);
            return false;
        }
    }
    if(!RebuildInstanceIndex(OutError))
    {
        Quarantine(OutError);
        return false;
    }
    TArray<FInventoryDomainRecord> FinalRecords;
    Records.Capture(FinalRecords);
    for(const FInventoryDomainRecord& Record : FinalRecords)
    {
        const TSharedRef<IInventoryDomainAdapter>* Adapter = Adapters.Find(Record.Key.Domain);
        if(Adapter && !(*Adapter)->Restore(Record, OutError))
        {
            Quarantine(OutError);
            return false;
        }
    }
    Transactions = MakeUnique<FInventoryTransactionService>(*this);
    bOpen = true;
    return true;
}

bool UInventoryModule::CaptureCheckpoint(FInventoryCheckpoint& OutCheckpoint, FString& OutError) const
{
    FInventoryCheckpoint Checkpoint;
    Checkpoint.WorldId = WorldId;
    Checkpoint.Sequence = Journal.GetSequence();
    Checkpoint.Inventories = Cold;
    Checkpoint.Tombstones = Tombstones;
    for(const TPair<FGuid, TObjectPtr<UAbilityInventoryBase>>& Pair : Active)
    {
        TArray<uint8> Bytes;
        if(Checkpoint.Inventories.Contains(Pair.Key) || !FInventorySnapshotCodec::Encode(Pair.Value->MakeSnapshot(), Registry, Profiles, Bytes, OutError))
        {
            return false;
        }
        Checkpoint.Inventories.Add(Pair.Key, MoveTemp(Bytes));
    }
    Records.Capture(Checkpoint.Domains);
    OutCheckpoint = MoveTemp(Checkpoint);
    return true;
}

bool UInventoryModule::SaveCheckpoint(FString& OutError)
{
    if(IsInGameThread() && Journal.IsTransient())
    {
        OutError = TEXT("Inventory.TransientArchiveUnsupported");
        return false;
    }
    if(!IsInGameThread() || !IsReady() || bCommitting || bPublishing || IsArchiveFrozen())
    {
        OutError = TEXT("Inventory.CheckpointNotReady");
        return false;
    }
    FInventoryCheckpoint Checkpoint;
    if(!CaptureCheckpoint(Checkpoint, OutError) || !Journal.Checkpoint(Checkpoint, OutError))
    {
        Quarantine(OutError);
        return false;
    }
    return true;
}

bool UInventoryModule::CaptureArchiveSnapshot(FInventoryArchiveSnapshot& OutSnapshot, FString& OutError) const
{
    if(IsInGameThread() && Journal.IsTransient())
    {
        OutError = TEXT("Inventory.TransientArchiveUnsupported");
        return false;
    }
    if(!IsInGameThread() || !IsReady() || bCommitting || bPublishing)
    {
        OutError = TEXT("Inventory.ArchiveCaptureNotReady");
        return false;
    }
    FInventoryCheckpoint Checkpoint;
    FInventoryArchiveSnapshot Snapshot;
    Snapshot.WorldId = WorldId;
    Snapshot.StoreId = StoreId;
    Snapshot.Sequence = Journal.GetSequence();
    if(!CaptureCheckpoint(Checkpoint, OutError) || !FInventoryJournal::EncodeCheckpoint(Checkpoint, Snapshot.CheckpointBytes, OutError)) return false;
    OutSnapshot = MoveTemp(Snapshot);
    return true;
}

bool UInventoryModule::FreezeArchive(const FGuid& InOperationId, FInventoryArchiveSnapshot& OutSnapshot, FString& OutError)
{
    if(IsInGameThread() && Journal.IsTransient())
    {
        OutError = TEXT("Inventory.TransientArchiveUnsupported");
        return false;
    }
    if(!IsInGameThread() || !InOperationId.IsValid() || !WorldEpoch.IsValid() || !StoreId.IsValid() || IsArchiveFrozen() || !IsReady() || bCommitting || bPublishing)
    {
        OutError = TEXT("Inventory.ArchiveFreezeNotReady");
        return false;
    }
    const FGuid OriginalEpoch = WorldEpoch;
    const FGuid OriginalStore = StoreId;
    const TSharedPtr<FInventoryArchiveFreeze, ESPMode::ThreadSafe> Freeze = ArchiveFreeze;
    if (!Freeze)
    {
        OutError = TEXT("Inventory.WorldUnavailable");
        return false;
    }
    Freeze->OperationId = InOperationId;
    Freeze->WorldEpoch = OriginalEpoch;
    Freeze->StoreId = OriginalStore;
    Freeze->Sequence = Journal.GetSequence();
    FInventoryArchiveSnapshot Snapshot;
    if(!CaptureArchiveSnapshot(Snapshot, OutError))
    {
        FString ReleaseError;
        if (!Freeze->Release(InOperationId, OriginalEpoch, OriginalStore, ReleaseError))
            OutError += TEXT("; Inventory freeze release failed: ") + ReleaseError;
        return false;
    }
    Freeze->CheckpointCrc = FCrc::MemCrc32(Snapshot.CheckpointBytes.GetData(), Snapshot.CheckpointBytes.Num());
    OutSnapshot = MoveTemp(Snapshot);
    return true;
}

bool UInventoryModule::ReleaseArchive(const FGuid& InOperationId, FString& OutError)
{
    if(IsInGameThread() && Journal.IsTransient())
    {
        OutError = TEXT("Inventory.TransientArchiveUnsupported");
        return false;
    }
    return ArchiveFreeze && ArchiveFreeze->Release(InOperationId, WorldEpoch, StoreId, OutError);
}

bool UInventoryModule::AdoptArchive(const FGuid& InOperationId, FInventoryPreparedStore& InPrepared, FString& OutError)
{
    if(IsInGameThread() && Journal.IsTransient())
    {
        OutError = TEXT("Inventory.TransientArchiveUnsupported");
        return false;
    }
    if(!IsInGameThread() || !IsReady() || bCommitting || bPublishing || !InOperationId.IsValid() || !ArchiveFreeze || ArchiveFreeze->OperationId != InOperationId ||
       ArchiveFreeze->WorldEpoch != WorldEpoch || ArchiveFreeze->StoreId != StoreId || ArchiveFreeze->Sequence != Journal.GetSequence() || !InPrepared.IsReady() || !InPrepared.bCommitted ||
       InPrepared.OperationId != InOperationId || InPrepared.WorldId != WorldId || InPrepared.SourceStoreId != StoreId ||
       InPrepared.StoreId == StoreId || InPrepared.Sequence != ArchiveFreeze->Sequence || InPrepared.CheckpointCrc != ArchiveFreeze->CheckpointCrc)
    {
        OutError = TEXT("Inventory.ArchiveAdoptionBoundaryMismatch");
        return false;
    }
    const FGuid OriginalEpoch = WorldEpoch;
    const TSharedPtr<FInventoryArchiveFreeze, ESPMode::ThreadSafe> Freeze = ArchiveFreeze;
    TGuardValue<bool> Publishing(bPublishing, true);
    Journal.SwapState(*InPrepared.Journal);
    const FGuid PreviousStoreId = StoreId;
    StoreId = InPrepared.StoreId;
    InPrepared.bAdopted = true;
    InPrepared.Close();
    TArray<FQueuedRequest> PreviousQueue = MoveTemp(Queue);
    for(int32 Index = 0; Index < PreviousQueue.Num(); ++Index)
    {
        FQueuedRequest& Entry = PreviousQueue[Index];
        if(Entry.Completion)
        {
            FInventoryCommandResult Result;
            Result.RequestId = Entry.Request.RequestId;
            Result.Code = EInventoryResultCode::GenerationChanged;
            Entry.Completion(Result);
        }
    }
    OnStoreAdopted.Broadcast();
    return Freeze->Release(InOperationId, OriginalEpoch, PreviousStoreId, OutError);
}

bool UInventoryModule::IsArchiveFrozen() const
{
    return ArchiveFreeze && ArchiveFreeze->IsFrozen();
}

bool UInventoryModule::DrainQueuedRequestsForClose(FString& OutError)
{
    if (!IsInGameThread() || bDrainingQueue || bCommitting || bPublishing)
    {
        OutError = TEXT("Inventory.QueueRetirementBlocked");
        return false;
    }
    FInventoryCommandResult Cancelled;
    Cancelled.Code = EInventoryResultCode::WorldNotReady;
    while (!Queue.IsEmpty())
    {
        FQueuedRequest Entry = MoveTemp(Queue[0]);
        Queue.RemoveAt(0, 1, EAllowShrinking::No);
        Cancelled.RequestId = Entry.Request.RequestId;
        if (Entry.Completion)
        {
            Entry.Completion(Cancelled);
        }
    }
    return true;
}

bool UInventoryModule::CloseStore(bool bInCheckpoint, FString& OutError)
{
    if(!IsInGameThread() || bCloseInProgress || bDrainingQueue || bCommitting || bPublishing ||
        (IsArchiveFrozen() && IsBusinessAdmissionOpen()))
    {
        OutError = TEXT("Inventory.CloseHasActiveCommit");
        return false;
    }
    if(bInCheckpoint && bOpen && Journal.IsTransient())
    {
        OutError = TEXT("Inventory.TransientArchiveUnsupported");
        return false;
    }
    TGuardValue<bool> ClosingGuard(bCloseInProgress, true);
    bClosing = true;
    if (!bCloseNotificationSent)
    {
        bCloseNotificationSent = true;
        OnClosing.Broadcast();
    }
    if (!DrainQueuedRequestsForClose(OutError)) return false;
    if (IsArchiveFrozen())
    {
        OutError = TEXT("Inventory.CloseHasActiveFreeze");
        return false;
    }
    if(!LeaseCounts.IsEmpty())
    {
        OutError = TEXT("Inventory.CloseHasActiveLease");
        return false;
    }
    if (bOpen && !Journal.IsTransient() && (bQuarantined || !Journal.IsHealthy()))
    {
        OutError = TEXT("Inventory.CheckpointUnavailable");
        return false;
    }
    if(bInCheckpoint && bOpen)
    {
        FInventoryCheckpoint StoreCheckpoint;
        if(!CaptureCheckpoint(StoreCheckpoint, OutError) || !Journal.Checkpoint(StoreCheckpoint, OutError))
        {
            Quarantine(OutError);
            return false;
        }
    }
    bOpen = false;
    WorldEpoch.Invalidate();
    Active.Reset();
    Cold.Reset();
    Tombstones.Reset();
    InstanceOwners.Reset();
    PendingRetire.Reset();
    FailedRetireReports.Reset();
    Records.Records.Reset();
    AccessBindings.Reset();
    Transactions.Reset();
    Journal.Close();
    WorldId.Invalidate();
    StoreId.Invalidate();
    OutError.Reset();
    return true;
}

bool UInventoryModule::IsReady() const
{
    return IsRuntimeBusinessOpen() && bOpen && !bQuarantined && !bClosing && Journal.IsHealthy();
}

bool UInventoryModule::IsRuntimeBusinessOpen() const
{
    return IsBusinessAdmissionOpen() && IsRuntimeBusinessAvailable();
}

bool UInventoryModule::CanRetireRuntime() const
{
    return IsInGameThread() && !bCommitting && !bPublishing && !bDrainingQueue && !bCloseInProgress;
}

bool UInventoryModule::IsQuarantined() const
{
    return bQuarantined;
}

const FString& UInventoryModule::GetFailureReason() const
{
    return FailureReason;
}

FGuid UInventoryModule::GetWorldId() const
{
    return WorldId;
}

FGuid UInventoryModule::GetStoreId() const
{
    return StoreId;
}

FGuid UInventoryModule::GetWorldEpoch() const
{
    return WorldEpoch;
}

const FInventoryItemRegistry& UInventoryModule::GetRegistry() const
{
    return Registry;
}

const TMap<FPrimaryAssetId, FInventoryProfileDefinition>& UInventoryModule::GetProfiles() const
{
    return Profiles;
}

const FInventoryRuleProvider* UInventoryModule::GetRules() const
{
    return Rules.Get();
}

const FInventoryRecordStore& UInventoryModule::GetRecords() const
{
    return Records;
}

UAbilityInventoryBase* UInventoryModule::Resolve(const FGuid& InInventoryId)
{
    if(!IsInGameThread() || !IsReady())
    {
        return nullptr;
    }
    if(const TObjectPtr<UAbilityInventoryBase>* Existing = Active.Find(InInventoryId))
    {
        return Existing->Get();
    }
    FInventorySnapshot Snapshot;
    FString Error;
    if(!GetSnapshot(InInventoryId, Snapshot, Error))
    {
        return nullptr;
    }
    UAbilityInventoryBase* Inventory = NewObject<UAbilityInventoryBase>(this);
    Inventory->State = MoveTemp(Snapshot);
    Inventory->OwnerModule = this;
    Cold.Remove(InInventoryId);
    Active.Add(InInventoryId, Inventory);
    return Inventory;
}

bool UInventoryModule::GetSnapshot(const FGuid& InInventoryId, FInventorySnapshot& OutSnapshot, FString& OutError) const
{
    if(!IsInGameThread() || bQuarantined)
    {
        OutError = TEXT("Inventory.WorldUnavailable");
        return false;
    }
    if(const TObjectPtr<UAbilityInventoryBase>* Inventory = Active.Find(InInventoryId))
    {
        OutSnapshot = (*Inventory)->MakeSnapshot();
        return true;
    }
    const TArray<uint8>* Bytes = Cold.Find(InInventoryId);
    if(!Bytes)
    {
        OutError = TEXT("Inventory.InventoryGone");
        return false;
    }
    return FInventorySnapshotCodec::Decode(*Bytes, Registry, Profiles, OutSnapshot, OutError);
}

bool UInventoryModule::Retire(const FGuid& InInventoryId, FString& OutError)
{
    if(!IsInGameThread() || !IsReady() || bCommitting || bPublishing || IsArchiveFrozen() || LeaseCounts.Contains(InInventoryId))
    {
        OutError = TEXT("Inventory.RetireBlocked");
        return false;
    }
    const TObjectPtr<UAbilityInventoryBase>* Inventory = Active.Find(InInventoryId);
    if(!Inventory)
    {
        return Cold.Contains(InInventoryId);
    }
    TArray<uint8> Bytes;
    if(!FInventorySnapshotCodec::Encode((*Inventory)->MakeSnapshot(), Registry, Profiles, Bytes, OutError))
    {
        return false;
    }
    Cold.Add(InInventoryId, MoveTemp(Bytes));
    Active.Remove(InInventoryId);
    OnRetired.Broadcast(InInventoryId);
    return true;
}

bool UInventoryModule::AcquireLease(const FGuid& InInventoryId)
{
    if(!Resolve(InInventoryId))
    {
        return false;
    }
    int32& Count = LeaseCounts.FindOrAdd(InInventoryId);
    if(Count == MAX_int32)
    {
        return false;
    }
    ++Count;
    PendingRetire.Remove(InInventoryId);
    return true;
}

void UInventoryModule::ReleaseLease(const FGuid& InInventoryId)
{
    int32* Count = LeaseCounts.Find(InInventoryId);
    if(!IsInGameThread() || !Count)
    {
        return;
    }
    if(--(*Count) == 0)
    {
        LeaseCounts.Remove(InInventoryId);
        PendingRetire.Add(InInventoryId);
    }
}

bool UInventoryModule::SetAccessBinding(FInventoryAccessBinding InBinding)
{
    if(!IsBusinessAdmissionOpen() || !IsInGameThread() || !InBinding.InventoryId.IsValid() || !InBinding.LifetimeOwner.IsValid() || !InBinding.Validator ||
       !FMath::IsFinite(InBinding.MaximumDistance) || InBinding.MaximumDistance <= 0 || InBinding.Location.ContainsNaN())
    {
        return false;
    }
    AccessBindings.Add(InBinding.InventoryId, MoveTemp(InBinding));
    return true;
}

void UInventoryModule::RemoveAccessBinding(const FGuid& InInventoryId)
{
    AccessBindings.Remove(InInventoryId);
}

EInventoryResultCode UInventoryModule::ValidateAccess(APlayerController* InController, const FGuid& InPlayerId,
    const FGuid& InInventoryId, bool bInWrite) const
{
    if(!IsReady() || !::IsValid(InController) || InController->GetWorld() != GetWorld() || !InPlayerId.IsValid())
    {
        return EInventoryResultCode::WorldNotReady;
    }
    const FInventoryAccessBinding* Binding = AccessBindings.Find(InInventoryId);
    if(!Binding || !Binding->LifetimeOwner.IsValid() || !Binding->Validator)
    {
        return EInventoryResultCode::NotInteractable;
    }
    const APawn* Pawn = InController->GetPawn();
    if(!::IsValid(Pawn) || Pawn->IsActorBeingDestroyed())
    {
        return EInventoryResultCode::InvalidState;
    }
    if(Binding->bDistanceLimited && FVector::DistSquared(Pawn->GetActorLocation(), Binding->Location) > FMath::Square(Binding->MaximumDistance))
    {
        return EInventoryResultCode::OutOfRange;
    }
    const FInventoryAccessValidator Validator = Binding->Validator;
    return Validator(InController, InPlayerId, bInWrite);
}

FInventoryAuthorityContext UInventoryModule::MakeServerAuthority(const FGuid& InActorId, FName InReason) const
{
    FInventoryAuthorityContext Context;
    if(IsInGameThread() && IsReady() && GetWorld()->GetNetMode() != NM_Client && InActorId.IsValid() && !InReason.IsNone())
    {
        Context.ActorId = InActorId;
        Context.WorldEpoch = WorldEpoch;
        Context.StoreId = StoreId;
        Context.Reason = InReason;
        Context.bServerDomain = true;
    }
    return Context;
}

FInventoryAuthorityContext UInventoryModule::MakeClientAuthority(APlayerController* InController, const FGuid& InActorId,
    const FGuid& InConnectionId, TConstArrayView<FGuid> InInventories) const
{
    FInventoryAuthorityContext Context;
    if(IsReady() && ::IsValid(InController) && InController->HasAuthority() && InController->GetWorld() == GetWorld() &&
       InActorId.IsValid() && InConnectionId.IsValid())
    {
        Context.ActorId = InActorId;
        Context.WorldEpoch = WorldEpoch;
        Context.StoreId = StoreId;
        Context.ConnectionId = InConnectionId;
        Context.Controller = InController;
        Context.Reason = TEXT("ClientInventoryCommand");
        for(const FGuid& InventoryId : InInventories)
        {
            Context.AllowedInventories.Add(InventoryId);
        }
    }
    return Context;
}

FInventoryCommandResult UInventoryModule::ExecuteAuthorizedBatch(const FInventoryBatchRequest& InRequest, const FInventoryAuthorityContext& InAuthority)
{
    if(!Transactions || !IsReady() || IsArchiveFrozen())
    {
        FInventoryCommandResult Result;
        Result.RequestId = InRequest.RequestId;
        Result.Code = bQuarantined ? EInventoryResultCode::SaveUnavailable : EInventoryResultCode::WorldNotReady;
        return Result;
    }
    if(InAuthority.StoreId != StoreId || InAuthority.WorldEpoch != WorldEpoch)
    {
        FInventoryCommandResult Result;
        Result.RequestId = InRequest.RequestId;
        Result.Code = EInventoryResultCode::GenerationChanged;
        return Result;
    }
    return Transactions->Execute(InRequest, InAuthority);
}

void UInventoryModule::EnqueueAuthorizedBatch(FInventoryBatchRequest InRequest, FInventoryAuthorityContext InAuthority,
    TFunction<void(const FInventoryCommandResult&)> InCompletion)
{
    if(!IsInGameThread() || !IsReady() || IsArchiveFrozen() || InAuthority.StoreId != StoreId || InAuthority.WorldEpoch != WorldEpoch || Queue.Num() >= 1024)
    {
        FInventoryCommandResult Result;
        Result.RequestId = InRequest.RequestId;
        Result.Code = !IsReady() || IsArchiveFrozen() ? EInventoryResultCode::WorldNotReady :
            InAuthority.StoreId != StoreId || InAuthority.WorldEpoch != WorldEpoch ? EInventoryResultCode::GenerationChanged : EInventoryResultCode::RateLimited;
        if(InCompletion)
        {
            InCompletion(Result);
        }
        return;
    }
    FQueuedRequest Entry;
    Entry.Request = MoveTemp(InRequest);
    Entry.Authority = MoveTemp(InAuthority);
    Entry.Completion = MoveTemp(InCompletion);
    Queue.Add(MoveTemp(Entry));
}

bool UInventoryModule::ValidateInstances(TConstArrayView<FInventorySnapshot> InBefore, TConstArrayView<FInventorySnapshot> InAfter, FString& OutError) const
{
    TSet<FGuid> ReplacedInventories;
    for(const FInventorySnapshot& Before : InBefore)
    {
        ReplacedInventories.Add(Before.InventoryId);
    }
    TSet<FGuid> NewInstances;
    for(const FInventorySnapshot& After : InAfter)
    {
        for(const FInventorySlotState& Slot : After.Slots)
        {
            const FGuid InstanceId = Slot.Item.InstanceId;
            if(!InstanceId.IsValid())
            {
                continue;
            }
            const FGuid* ExistingOwner = InstanceOwners.Find(InstanceId);
            if(NewInstances.Contains(InstanceId) || (ExistingOwner && !ReplacedInventories.Contains(*ExistingOwner)))
            {
                OutError = TEXT("Inventory.DuplicateInstance");
                return false;
            }
            NewInstances.Add(InstanceId);
        }
    }
    return true;
}

bool UInventoryModule::RebuildInstanceIndex(FString& OutError)
{
    TMap<FGuid, FGuid> NewIndex;
    TArray<FGuid> InventoryIds;
    Cold.GetKeys(InventoryIds);
    for(const TPair<FGuid, TObjectPtr<UAbilityInventoryBase>>& Pair : Active)
    {
        InventoryIds.Add(Pair.Key);
    }
    InventoryIds.Sort(InventoryOrder::GuidLess);
    for(const FGuid& InventoryId : InventoryIds)
    {
        FInventorySnapshot Snapshot;
        if(!GetSnapshot(InventoryId, Snapshot, OutError) || Snapshot.InventoryId != InventoryId || Snapshot.WorldId != WorldId ||
           Snapshot.Revision == 0 || Tombstones.Contains(InventoryId))
        {
            OutError = TEXT("Inventory.StoredIdentityInvalid");
            return false;
        }
        for(const FInventorySlotState& Slot : Snapshot.Slots)
        {
            if(Slot.Item.InstanceId.IsValid())
            {
                if(NewIndex.Contains(Slot.Item.InstanceId))
                {
                    OutError = TEXT("Inventory.DuplicateInstance");
                    return false;
                }
                NewIndex.Add(Slot.Item.InstanceId, InventoryId);
            }
        }
    }
    InstanceOwners = MoveTemp(NewIndex);
    return true;
}

void UInventoryModule::Quarantine(const FString& InReason)
{
    bQuarantined = true;
    FailureReason = InReason;
    if (!bCloseNotificationSent)
    {
        bCloseNotificationSent = true;
        OnClosing.Broadcast();
    }
    UE_LOG(LogTemp, Error, TEXT("Inventory world quarantined: %s"), *FailureReason);
}

void UInventoryModule::ReportProjectionFailure(const FString& InReason)
{
    if(IsInGameThread() && bOpen && !bQuarantined && GetWorld() && GetWorld()->GetNetMode() != NM_Client)
    {
        Quarantine(InReason);
    }
}
