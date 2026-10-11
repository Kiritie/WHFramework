#include "Ability/Inventory/InventoryWorldSubsystem.h"

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

UInventoryWorldSubsystem::UInventoryWorldSubsystem()
{
}

UInventoryWorldSubsystem::~UInventoryWorldSubsystem()
{
}

void UInventoryWorldSubsystem::Deinitialize()
{
    ArchiveOperationId.Invalidate();
    ArchiveWorldEpoch.Invalidate();
    ArchiveSequence = 0;
    ArchiveCheckpointCrc = 0;
    FString Error;
    if(bOpen && !CloseStore(true, Error))
    {
        UE_LOG(LogTemp, Error, TEXT("Inventory close: %s"), *Error);
    }
    OnClosing.Broadcast();
    Queue.Reset();
    Active.Reset();
    Cold.Reset();
    LeaseCounts.Reset();
    AccessBindings.Reset();
    InstanceOwners.Reset();
    Transactions.Reset();
    Adapters.Reset();
    Journal.Close();
    bOpen = false;
    Super::Deinitialize();
}

void UInventoryWorldSubsystem::Tick(float InDeltaTime)
{
    if(!IsReady() || bCommitting || bPublishing || IsArchiveFrozen())
    {
        return;
    }
    const TArray<FGuid> Retiring = PendingRetire.Array();
    PendingRetire.Reset();
    for(const FGuid& InventoryId : Retiring)
    {
        FString Error;
        if(!LeaseCounts.Contains(InventoryId) && !Tombstones.Contains(InventoryId))
        {
            Retire(InventoryId, Error);
        }
    }
    if(Queue.IsEmpty())
    {
        return;
    }
    const int32 Count = FMath::Min(Queue.Num(), 32);
    TArray<FQueuedRequest> Current;
    Current.Reserve(Count);
    for(int32 RequestIndex = 0; RequestIndex < Count; ++RequestIndex)
    {
        Current.Add(MoveTemp(Queue[RequestIndex]));
    }
    Queue.RemoveAt(0, Count, EAllowShrinking::No);
    for(int32 RequestIndex = 0; RequestIndex < Current.Num(); ++RequestIndex)
    {
        if(IsArchiveFrozen())
        {
            // 回调可在本轮队列中间开始异步另存；尚未执行的请求仍属于原分支。
            TArray<FQueuedRequest> Deferred;
            Deferred.Reserve(Current.Num() - RequestIndex + Queue.Num());
            for(int32 PendingIndex = RequestIndex; PendingIndex < Current.Num(); ++PendingIndex)
            {
                Deferred.Add(MoveTemp(Current[PendingIndex]));
            }
            for(FQueuedRequest& Pending : Queue) Deferred.Add(MoveTemp(Pending));
            Queue = MoveTemp(Deferred);
            break;
        }
        FQueuedRequest& Entry = Current[RequestIndex];
        const FInventoryCommandResult Result = ExecuteAuthorizedBatch(Entry.Request, Entry.Authority);
        if(Entry.Completion)
        {
            Entry.Completion(Result);
        }
    }
}

TStatId UInventoryWorldSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UInventoryWorldSubsystem, STATGROUP_Tickables);
}

bool UInventoryWorldSubsystem::IsTickable() const
{
    return !IsTemplate() && bOpen && !IsArchiveFrozen() && (!Queue.IsEmpty() || !PendingRetire.IsEmpty());
}

bool UInventoryWorldSubsystem::Configure(TConstArrayView<FInventoryItemDefinition> InItems, TConstArrayView<FInventoryProfileDefinition> InProfiles,
    TSharedPtr<FInventoryRuleProvider> InRules, FString& OutError)
{
    if(!IsInGameThread() || bOpen || bConfigured || InProfiles.IsEmpty())
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

bool UInventoryWorldSubsystem::SetItemAssets(UInventoryItemAssetCatalog* InAssets)
{
    if(bOpen || !IsInGameThread() || !IsValid(InAssets))
    {
        return false;
    }
    ItemAssets = InAssets;
    return true;
}

const UInventoryItemAssetCatalog* UInventoryWorldSubsystem::GetItemAssets() const
{
    return ItemAssets;
}

bool UInventoryWorldSubsystem::RegisterRecordDomain(FName InDomain, int32 InMaximumBytes, FInventoryRecordValidator InValidator, FString& OutError)
{
    if(bOpen || !IsInGameThread())
    {
        OutError = TEXT("Inventory.DomainRegistrationClosed");
        return false;
    }
    return Records.RegisterDomain(InDomain, InMaximumBytes, MoveTemp(InValidator), OutError);
}

bool UInventoryWorldSubsystem::RegisterDomainAdapter(TSharedRef<IInventoryDomainAdapter> InAdapter, FString& OutError)
{
    if(bOpen || !IsInGameThread() || InAdapter->GetDomain().IsNone() || Adapters.Contains(InAdapter->GetDomain()))
    {
        OutError = TEXT("Inventory.DomainAdapterInvalid");
        return false;
    }
    Adapters.Add(InAdapter->GetDomain(), InAdapter);
    return true;
}

bool UInventoryWorldSubsystem::RegisterSnapshotProjector(FName InName, FInventorySnapshotProjector InProjector, FString& OutError)
{
    if(bOpen || !IsInGameThread() || InName.IsNone() || !InProjector || SnapshotProjectors.Contains(InName))
    {
        OutError = TEXT("Inventory.SnapshotProjectorInvalid");
        return false;
    }
    SnapshotProjectors.Add(InName, MoveTemp(InProjector));
    return true;
}

bool UInventoryWorldSubsystem::OpenStore(const FGuid& InWorldId, const FGuid& InStoreId, const FString& InDirectory, bool bInCreateNew, FString& OutError)
{
    if(!IsInGameThread() || !bConfigured || bOpen || !GetWorld() || GetWorld()->GetNetMode() == NM_Client)
    {
        OutError = TEXT("Inventory.OpenStoreInvalid");
        return false;
    }
    bQuarantined = false;
    bClosing = false;
    FailureReason.Reset();
    FInventoryCheckpoint Checkpoint;
    TArray<FInventoryJournalRecord> JournalRecords;
    if(!Journal.Open(InDirectory, InWorldId, InStoreId, bInCreateNew, Checkpoint, JournalRecords, OutError))
    {
        Quarantine(OutError);
        return false;
    }
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

bool UInventoryWorldSubsystem::CaptureCheckpoint(FInventoryCheckpoint& OutCheckpoint, FString& OutError) const
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

bool UInventoryWorldSubsystem::SaveCheckpoint(FString& OutError)
{
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

bool UInventoryWorldSubsystem::CaptureArchiveSnapshot(FInventoryArchiveSnapshot& OutSnapshot, FString& OutError) const
{
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

bool UInventoryWorldSubsystem::FreezeArchive(const FGuid& InOperationId, FInventoryArchiveSnapshot& OutSnapshot, FString& OutError)
{
    if(!IsInGameThread() || !InOperationId.IsValid() || IsArchiveFrozen() || !IsReady() || bCommitting || bPublishing)
    {
        OutError = TEXT("Inventory.ArchiveFreezeNotReady");
        return false;
    }
    ArchiveOperationId = InOperationId;
    ArchiveWorldEpoch = WorldEpoch;
    ArchiveSequence = Journal.GetSequence();
    if(!CaptureArchiveSnapshot(OutSnapshot, OutError))
    {
        ArchiveOperationId.Invalidate();
        ArchiveWorldEpoch.Invalidate();
        ArchiveSequence = 0;
        ArchiveCheckpointCrc = 0;
        return false;
    }
    ArchiveCheckpointCrc = FCrc::MemCrc32(OutSnapshot.CheckpointBytes.GetData(), OutSnapshot.CheckpointBytes.Num());
    return true;
}

bool UInventoryWorldSubsystem::ReleaseArchive(const FGuid& InOperationId, FString& OutError)
{
    if(!IsInGameThread() || !InOperationId.IsValid() || ArchiveOperationId != InOperationId || ArchiveWorldEpoch != WorldEpoch)
    {
        OutError = TEXT("Inventory.ArchiveFreezeOwnerMismatch");
        return false;
    }
    ArchiveOperationId.Invalidate();
    ArchiveWorldEpoch.Invalidate();
    ArchiveSequence = 0;
    ArchiveCheckpointCrc = 0;
    return true;
}

bool UInventoryWorldSubsystem::AdoptArchive(const FGuid& InOperationId, FInventoryPreparedStore& InPrepared, FString& OutError)
{
    if(!IsInGameThread() || !IsReady() || bCommitting || bPublishing || !InOperationId.IsValid() || ArchiveOperationId != InOperationId ||
       ArchiveWorldEpoch != WorldEpoch || ArchiveSequence != Journal.GetSequence() || !InPrepared.IsReady() || !InPrepared.bCommitted ||
       InPrepared.OperationId != InOperationId || InPrepared.WorldId != WorldId || InPrepared.SourceStoreId != StoreId ||
       InPrepared.StoreId == StoreId || InPrepared.Sequence != ArchiveSequence || InPrepared.CheckpointCrc != ArchiveCheckpointCrc)
    {
        OutError = TEXT("Inventory.ArchiveAdoptionBoundaryMismatch");
        return false;
    }
    TGuardValue<bool> Publishing(bPublishing, true);
    Journal.SwapState(*InPrepared.Journal);
    StoreId = InPrepared.StoreId;
    InPrepared.bAdopted = true;
    InPrepared.Close();
    TArray<FQueuedRequest> PreviousQueue = MoveTemp(Queue);
    for(FQueuedRequest& Entry : PreviousQueue)
    {
        if(Entry.Completion)
        {
            FInventoryCommandResult Result;
            Result.RequestId = Entry.Request.RequestId;
            Result.Code = EInventoryResultCode::GenerationChanged;
            Entry.Completion(Result);
        }
    }
    OnStoreAdopted.Broadcast();
    ArchiveOperationId.Invalidate();
    ArchiveWorldEpoch.Invalidate();
    ArchiveSequence = 0;
    ArchiveCheckpointCrc = 0;
    return true;
}

bool UInventoryWorldSubsystem::IsArchiveFrozen() const
{
    return ArchiveOperationId.IsValid();
}

bool UInventoryWorldSubsystem::CloseStore(bool bInCheckpoint, FString& OutError)
{
    if(!IsInGameThread() || bCommitting || bPublishing || IsArchiveFrozen())
    {
        OutError = TEXT("Inventory.CloseHasActiveCommit");
        return false;
    }
    bClosing = true;
    OnClosing.Broadcast();
    if(!LeaseCounts.IsEmpty())
    {
        OutError = TEXT("Inventory.CloseHasActiveLease");
        return false;
    }
    if(bInCheckpoint && bOpen && !bQuarantined && Journal.IsHealthy())
    {
        FInventoryCheckpoint Checkpoint;
        if(!CaptureCheckpoint(Checkpoint, OutError) || !Journal.Checkpoint(Checkpoint, OutError))
        {
            Quarantine(OutError);
            return false;
        }
    }
    bOpen = false;
    WorldEpoch.Invalidate();
    FInventoryCommandResult Cancelled;
    Cancelled.Code = EInventoryResultCode::WorldNotReady;
    TArray<FQueuedRequest> CancelledQueue = MoveTemp(Queue);
    for(FQueuedRequest& Entry : CancelledQueue)
    {
        Cancelled.RequestId = Entry.Request.RequestId;
        if(Entry.Completion)
        {
            Entry.Completion(Cancelled);
        }
    }
    Active.Reset();
    Cold.Reset();
    Tombstones.Reset();
    InstanceOwners.Reset();
    PendingRetire.Reset();
    Records.Records.Reset();
    AccessBindings.Reset();
    Transactions.Reset();
    Journal.Close();
    WorldId.Invalidate();
    StoreId.Invalidate();
    return true;
}

bool UInventoryWorldSubsystem::IsReady() const
{
    return bOpen && !bQuarantined && !bClosing && Journal.IsHealthy();
}

bool UInventoryWorldSubsystem::IsQuarantined() const
{
    return bQuarantined;
}

const FString& UInventoryWorldSubsystem::GetFailureReason() const
{
    return FailureReason;
}

FGuid UInventoryWorldSubsystem::GetWorldId() const
{
    return WorldId;
}

FGuid UInventoryWorldSubsystem::GetStoreId() const
{
    return StoreId;
}

FGuid UInventoryWorldSubsystem::GetWorldEpoch() const
{
    return WorldEpoch;
}

const FInventoryItemRegistry& UInventoryWorldSubsystem::GetRegistry() const
{
    return Registry;
}

const TMap<FPrimaryAssetId, FInventoryProfileDefinition>& UInventoryWorldSubsystem::GetProfiles() const
{
    return Profiles;
}

const FInventoryRuleProvider* UInventoryWorldSubsystem::GetRules() const
{
    return Rules.Get();
}

const FInventoryRecordStore& UInventoryWorldSubsystem::GetRecords() const
{
    return Records;
}

UAbilityInventoryBase* UInventoryWorldSubsystem::Resolve(const FGuid& InInventoryId)
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
    Inventory->OwnerSubsystem = this;
    Cold.Remove(InInventoryId);
    Active.Add(InInventoryId, Inventory);
    return Inventory;
}

bool UInventoryWorldSubsystem::GetSnapshot(const FGuid& InInventoryId, FInventorySnapshot& OutSnapshot, FString& OutError) const
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

bool UInventoryWorldSubsystem::Retire(const FGuid& InInventoryId, FString& OutError)
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

bool UInventoryWorldSubsystem::AcquireLease(const FGuid& InInventoryId)
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

void UInventoryWorldSubsystem::ReleaseLease(const FGuid& InInventoryId)
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

bool UInventoryWorldSubsystem::SetAccessBinding(FInventoryAccessBinding InBinding)
{
    if(!IsInGameThread() || !InBinding.InventoryId.IsValid() || !InBinding.LifetimeOwner.IsValid() || !InBinding.Validator ||
       !FMath::IsFinite(InBinding.MaximumDistance) || InBinding.MaximumDistance <= 0 || InBinding.Location.ContainsNaN())
    {
        return false;
    }
    AccessBindings.Add(InBinding.InventoryId, MoveTemp(InBinding));
    return true;
}

void UInventoryWorldSubsystem::RemoveAccessBinding(const FGuid& InInventoryId)
{
    AccessBindings.Remove(InInventoryId);
}

EInventoryResultCode UInventoryWorldSubsystem::ValidateAccess(APlayerController* InController, const FGuid& InPlayerId,
    const FGuid& InInventoryId, bool bInWrite) const
{
    if(!IsReady() || !IsValid(InController) || InController->GetWorld() != GetWorld() || !InPlayerId.IsValid())
    {
        return EInventoryResultCode::WorldNotReady;
    }
    const FInventoryAccessBinding* Binding = AccessBindings.Find(InInventoryId);
    if(!Binding || !Binding->LifetimeOwner.IsValid() || !Binding->Validator)
    {
        return EInventoryResultCode::NotInteractable;
    }
    const APawn* Pawn = InController->GetPawn();
    if(!IsValid(Pawn) || Pawn->IsActorBeingDestroyed())
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

FInventoryAuthorityContext UInventoryWorldSubsystem::MakeServerAuthority(const FGuid& InActorId, FName InReason) const
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

FInventoryAuthorityContext UInventoryWorldSubsystem::MakeClientAuthority(APlayerController* InController, const FGuid& InActorId,
    const FGuid& InConnectionId, TConstArrayView<FGuid> InInventories) const
{
    FInventoryAuthorityContext Context;
    if(IsReady() && IsValid(InController) && InController->HasAuthority() && InController->GetWorld() == GetWorld() &&
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

FInventoryCommandResult UInventoryWorldSubsystem::ExecuteAuthorizedBatch(const FInventoryBatchRequest& InRequest, const FInventoryAuthorityContext& InAuthority)
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

void UInventoryWorldSubsystem::EnqueueAuthorizedBatch(FInventoryBatchRequest InRequest, FInventoryAuthorityContext InAuthority,
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

bool UInventoryWorldSubsystem::ValidateInstances(TConstArrayView<FInventorySnapshot> InBefore, TConstArrayView<FInventorySnapshot> InAfter, FString& OutError) const
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

bool UInventoryWorldSubsystem::RebuildInstanceIndex(FString& OutError)
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

void UInventoryWorldSubsystem::Quarantine(const FString& InReason)
{
    bQuarantined = true;
    FailureReason = InReason;
    OnClosing.Broadcast();
    UE_LOG(LogTemp, Error, TEXT("Inventory world quarantined: %s"), *FailureReason);
}

void UInventoryWorldSubsystem::ReportProjectionFailure(const FString& InReason)
{
    if(IsInGameThread() && bOpen && !bQuarantined && GetWorld() && GetWorld()->GetNetMode() != NM_Client)
    {
        Quarantine(InReason);
    }
}
