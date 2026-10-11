#include "Ability/Inventory/InventoryNetworkComponent.h"

#include "Ability/Inventory/InventorySnapshotCodec.h"
#include "Ability/Inventory/InventoryTransactionService.h"
#include "Ability/Inventory/InventoryWorldSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Misc/Crc.h"
#include "Serialization/MemoryWriter.h"

namespace InventoryNetworkFormat
{
    constexpr int32 ChunkBytes = 8192;
    constexpr int32 MaximumParticipants = 8;

    bool SerializeName(FArchive& InArchive, FName& InOutName)
    {
        uint16 ByteCount = 0;
        if(InArchive.IsSaving())
        {
            const FString Value = InOutName.ToString();
            if(Value.Len() > 128)
            {
                return false;
            }
            const FTCHARToUTF8 Utf8(*Value);
            if(Utf8.Length() > 512)
            {
                return false;
            }
            ByteCount = static_cast<uint16>(Utf8.Length());
            InArchive << ByteCount;
            InArchive.Serialize(const_cast<ANSICHAR*>(Utf8.Get()), ByteCount);
            return !InArchive.IsError();
        }
        InArchive << ByteCount;
        if(InArchive.IsError() || ByteCount == 0 || ByteCount > 512)
        {
            return false;
        }
        ANSICHAR Bytes[513] = {};
        InArchive.Serialize(Bytes, ByteCount);
        if(InArchive.IsError())
        {
            return false;
        }
        for(uint16 ByteIndex = 0; ByteIndex < ByteCount; ++ByteIndex)
        {
            if(Bytes[ByteIndex] == 0)
            {
                return false;
            }
        }
        const FUTF8ToTCHAR Converted(Bytes, ByteCount);
        if(Converted.Length() > 128)
        {
            return false;
        }
        const FString Value(Converted.Length(), Converted.Get());
        const FTCHARToUTF8 RoundTrip(*Value);
        if(RoundTrip.Length() != ByteCount || FMemory::Memcmp(RoundTrip.Get(), Bytes, ByteCount) != 0)
        {
            return false;
        }
        InOutName = FName(*Value);
        return true;
    }

    bool SerializeAsset(FArchive& InArchive, FPrimaryAssetId& InOutId)
    {
        FName Type = InOutId.PrimaryAssetType.GetName();
        FName Name = InOutId.PrimaryAssetName;
        if(!SerializeName(InArchive, Type) || !SerializeName(InArchive, Name))
        {
            return false;
        }
        if(InArchive.IsLoading())
        {
            InOutId = FPrimaryAssetId(FPrimaryAssetType(Type), Name);
        }
        return true;
    }

    bool SerializeTag(FArchive& InArchive, FGameplayTag& InOutTag)
    {
        FName Name = InOutTag.GetTagName();
        if(!SerializeName(InArchive, Name))
        {
            return false;
        }
        if(InArchive.IsLoading())
        {
            InOutTag = Name.IsNone() ? FGameplayTag() : FGameplayTag::RequestGameplayTag(Name, false);
            if(!Name.IsNone() && !InOutTag.IsValid())
            {
                return false;
            }
        }
        return true;
    }

    bool SerializeSlot(FArchive& InArchive, FInventorySlotKey& InOutSlot)
    {
        if(!SerializeTag(InArchive, InOutSlot.Group))
        {
            return false;
        }
        InArchive << InOutSlot.Index;
        return !InArchive.IsError() && InOutSlot.Index >= INDEX_NONE && InOutSlot.Index < FInventorySnapshotCodec::MaximumSlots;
    }

    bool SerializeSignature(FArchive& InArchive, FInventoryStackSignature& InOutSignature)
    {
        if(!SerializeAsset(InArchive, InOutSignature.ItemId))
        {
            return false;
        }
        InArchive << InOutSignature.Level;
        InArchive << InOutSignature.InstanceId;
        if(InOutSignature.Properties.Num() > FInventorySnapshotCodec::MaximumProperties)
        {
            return false;
        }
        uint8 Count = static_cast<uint8>(InOutSignature.Properties.Num());
        InArchive << Count;
        if(Count > FInventorySnapshotCodec::MaximumProperties)
        {
            return false;
        }
        if(InArchive.IsLoading())
        {
            InOutSignature.Properties.SetNum(Count);
        }
        for(FInventoryItemProperty& Property : InOutSignature.Properties)
        {
            if(InArchive.IsLoading())
            {
                Property = FInventoryItemProperty();
            }
            if(!SerializeName(InArchive, Property.Key))
            {
                return false;
            }
            InArchive << Property.SchemaVersion;
            uint8 Type = static_cast<uint8>(Property.Type);
            InArchive << Type;
            Property.Type = static_cast<EInventoryPropertyType>(Type);
            switch(Property.Type)
            {
                case EInventoryPropertyType::Integer:
                    InArchive << Property.IntValue;
                    break;
                case EInventoryPropertyType::Boolean:
                    InArchive << Property.BoolValue;
                    break;
                case EInventoryPropertyType::Guid:
                    InArchive << Property.GuidValue;
                    break;
                case EInventoryPropertyType::Name:
                    if(!SerializeName(InArchive, Property.NameValue))
                    {
                        return false;
                    }
                    break;
                default:
                    return false;
            }
            if(!Property.HasCanonicalFields())
            {
                return false;
            }
        }
        return !InArchive.IsError();
    }

    bool IsClientOperation(EInventoryOperationType InType)
    {
        return InType == EInventoryOperationType::Move || InType == EInventoryOperationType::Split ||
            InType == EInventoryOperationType::Merge || InType == EInventoryOperationType::Swap ||
            InType == EInventoryOperationType::Transfer || InType == EInventoryOperationType::Sort ||
            InType == EInventoryOperationType::QuickStack || InType == EInventoryOperationType::SetFavorite;
    }
}

bool FInventoryClientRequest::NetSerialize(FArchive& InArchive, UPackageMap* InMap, bool& bOutSuccess)
{
    bOutSuccess = false;
    InArchive << RequestId;
    InArchive << ConnectionId;
    InArchive << StoreId;
    InArchive << WorldEpoch;
    if(!InventoryNetworkFormat::SerializeName(InArchive, DomainAction))
    {
        return true;
    }
    uint8 OperationValue = static_cast<uint8>(Operation);
    InArchive << OperationValue;
    Operation = static_cast<EInventoryOperationType>(OperationValue);
    InArchive << SourceInventoryId;
    InArchive << TargetInventoryId;
    if(TargetInventories.Num() > InventoryNetworkFormat::MaximumParticipants)
    {
        return true;
    }
    uint8 TargetCount = static_cast<uint8>(TargetInventories.Num());
    InArchive << TargetCount;
    if(TargetCount > InventoryNetworkFormat::MaximumParticipants)
    {
        return true;
    }
    if(InArchive.IsLoading())
    {
        TargetInventories.SetNum(TargetCount);
    }
    for(FGuid& InventoryId : TargetInventories)
    {
        InArchive << InventoryId;
    }
    if(!InventoryNetworkFormat::SerializeSlot(InArchive, SourceSlot) || !InventoryNetworkFormat::SerializeSlot(InArchive, TargetSlot) ||
       !InventoryNetworkFormat::SerializeTag(InArchive, SourceGroup) || !InventoryNetworkFormat::SerializeTag(InArchive, TargetGroup))
    {
        return true;
    }
    InArchive << Quantity;
    InArchive << bAllowPartial;
    InArchive << bFavorite;
    InArchive << bAutomatic;
    if(!InventoryNetworkFormat::SerializeAsset(InArchive, DefinitionId))
    {
        return true;
    }
    InArchive << TargetEntityId;
    InArchive << TargetLocation;
    if(TargetLocation.ContainsNaN() || !InventoryNetworkFormat::SerializeSignature(InArchive, ExpectedSignature) ||
       Participants.Num() > InventoryNetworkFormat::MaximumParticipants)
    {
        return true;
    }
    uint8 ParticipantCount = static_cast<uint8>(Participants.Num());
    InArchive << ParticipantCount;
    if(ParticipantCount > InventoryNetworkFormat::MaximumParticipants)
    {
        return true;
    }
    if(InArchive.IsLoading())
    {
        Participants.SetNum(ParticipantCount);
    }
    for(FInventoryClientParticipant& Participant : Participants)
    {
        InArchive << Participant.InventoryId;
        InArchive << Participant.SessionId;
        InArchive << Participant.Revision;
        InArchive << Participant.Generation;
    }
    bOutSuccess = !InArchive.IsError();
    return true;
}

bool FInventorySnapshotChunk::NetSerialize(FArchive& InArchive, UPackageMap* InMap, bool& bOutSuccess)
{
    bOutSuccess = false;
    InArchive << ConnectionId;
    InArchive << StoreId;
    InArchive << WorldEpoch;
    InArchive << SessionId;
    InArchive << InventoryId;
    InArchive << TransferId;
    InArchive << Revision;
    InArchive << Generation;
    InArchive << Index;
    InArchive << Count;
    InArchive << TotalBytes;
    InArchive << Checksum;
    if(TotalBytes < 1 || TotalBytes > FInventorySnapshotCodec::MaximumBytes || Count < 1 ||
       Count != FMath::DivideAndRoundUp(TotalBytes, InventoryNetworkFormat::ChunkBytes) || Index < 0 || Index >= Count)
    {
        return true;
    }
    int32 ByteCount = Bytes.Num();
    InArchive << ByteCount;
    const int32 Expected = FMath::Min(InventoryNetworkFormat::ChunkBytes, TotalBytes - Index * InventoryNetworkFormat::ChunkBytes);
    if(ByteCount != Expected || ByteCount < 1 || ByteCount > InventoryNetworkFormat::ChunkBytes)
    {
        return true;
    }
    if(InArchive.IsLoading())
    {
        Bytes.SetNumUninitialized(ByteCount);
    }
    InArchive.Serialize(Bytes.GetData(), ByteCount);
    bOutSuccess = !InArchive.IsError();
    return true;
}

UInventoryNetworkComponent::UInventoryNetworkComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = true;
}

void UInventoryNetworkComponent::BeginPlay()
{
    Super::BeginPlay();
    if(!Cast<APlayerController>(GetOwner()))
    {
        SetComponentTickEnabled(false);
        return;
    }
    WorldSubsystem = GetWorld()->GetSubsystem<UInventoryWorldSubsystem>();
    if(UInventoryWorldSubsystem* World = WorldSubsystem.Get())
    {
        if(GetOwner()->HasAuthority())
        {
            CommitHandle = World->OnCommitted.AddUObject(this, &UInventoryNetworkComponent::HandleCommit);
            DeleteHandle = World->OnDeleted.AddUObject(this, &UInventoryNetworkComponent::HandleDeleted);
            ClosingHandle = World->OnClosing.AddUObject(this, &UInventoryNetworkComponent::HandleClosing);
            AdoptedHandle = World->OnStoreAdopted.AddUObject(this, &UInventoryNetworkComponent::HandleStoreAdopted);
        }
    }
}

void UInventoryNetworkComponent::EndPlay(const EEndPlayReason::Type InEndPlayReason)
{
    HandleClosing();
    if(UInventoryWorldSubsystem* World = WorldSubsystem.Get())
    {
        World->OnCommitted.Remove(CommitHandle);
        World->OnDeleted.Remove(DeleteHandle);
        World->OnClosing.Remove(ClosingHandle);
        World->OnStoreAdopted.Remove(AdoptedHandle);
        if(World->Transactions)
        {
            World->Transactions->ForgetConnection(ConnectionId);
        }
    }
    Actions.Reset();
    Pending.Reset();
    Views.Reset();
    Assemblies.Reset();
    Super::EndPlay(InEndPlayReason);
}

void UInventoryNetworkComponent::TickComponent(float InDeltaTime, ELevelTick InTickType, FActorComponentTickFunction* InTickFunction)
{
    Super::TickComponent(InDeltaTime, InTickType, InTickFunction);
    UInventoryWorldSubsystem* World = WorldSubsystem.Get();
    APlayerController* Controller = Cast<APlayerController>(GetOwner());
    if(!World || !Controller)
    {
        return;
    }
    if(Controller->HasAuthority())
    {
        SessionCheckAccumulator += InDeltaTime;
        if(SessionCheckAccumulator >= .25f)
        {
            SessionCheckAccumulator = 0;
            TArray<FGuid> InventoryIds;
            Sessions.GetKeys(InventoryIds);
            for(const FGuid& InventoryId : InventoryIds)
            {
                const EInventoryResultCode Access = World->ValidateAccess(Controller, PlayerId, InventoryId, false);
                if(Access != EInventoryResultCode::Success)
                {
                    CloseServerSession(InventoryId, Access);
                }
            }
        }
    }
    if(Controller->IsLocalController())
    {
        const double Now = FPlatformTime::Seconds();
        TArray<uint64> RequestIds;
        Pending.GetKeys(RequestIds);
        RequestIds.Sort();
        for(uint64 RequestId : RequestIds)
        {
            FPendingRequest* Entry = Pending.Find(RequestId);
            if(!Entry || (Entry->Attempts > 0 && Now - Entry->SentAt < 2.0))
            {
                continue;
            }
            if(Entry->Attempts >= 4)
            {
                FInventoryCommandResult Result;
                Result.RequestId = RequestId;
                Result.Code = EInventoryResultCode::ConfirmationPending;
                const TArray<FInventoryClientParticipant> Participants = Entry->Request.Participants;
                Pending.Remove(RequestId);
                OnResult.Broadcast(Result);
                for(const FInventoryClientParticipant& Participant : Participants)
                {
                    ResyncInventory(Participant.InventoryId);
                }
                continue;
            }
            ++Entry->Attempts;
            Entry->SentAt = Now;
            const FInventoryClientRequest Request = Entry->Request;
            ServerSubmit(Request);
        }
        TArray<FGuid> ExpiredAssemblies;
        for(const TPair<FGuid, FAssembly>& Pair : Assemblies)
        {
            if(Now - Pair.Value.StartedAt > 10.0)
            {
                ExpiredAssemblies.Add(Pair.Key);
            }
        }
        for(const FGuid& TransferId : ExpiredAssemblies)
        {
            const FAssembly* Assembly = Assemblies.Find(TransferId);
            if(Assembly)
            {
                ServerResync(Assembly->Header.InventoryId, Assembly->Header.SessionId);
                Assemblies.Remove(TransferId);
            }
        }
    }
}

bool UInventoryNetworkComponent::BindOwner(const FGuid& InPlayerId, const FGuid& InInventoryId)
{
    UInventoryWorldSubsystem* World = WorldSubsystem.Get();
    if(bResettingConnection || !World || !GetOwner() || !GetOwner()->HasAuthority() || !World->IsReady() || !InPlayerId.IsValid())
    {
        return false;
    }
    FInventorySnapshot Snapshot;
    FString Error;
    if(!World->GetSnapshot(InInventoryId, Snapshot, Error) || Snapshot.OwnerId != InPlayerId)
    {
        return false;
    }
    UnbindOwner();
    PlayerId = InPlayerId;
    OwnerInventoryId = InInventoryId;
    ConnectionId = FGuid::NewGuid();
    WorldEpoch = World->GetWorldEpoch();
    ConnectionWorldId = World->GetWorldId();
    ConnectionStoreId = World->GetStoreId();
    BudgetUpdatedAt = FPlatformTime::Seconds();
    Tokens = 40;
    Receipts.Reset();
    RequestHighWaterMark = 0;
    ClientBegin(ConnectionId, ConnectionWorldId, ConnectionStoreId, WorldEpoch, OwnerInventoryId);
    ServerOpen_Implementation(ConnectionId, ConnectionStoreId, InInventoryId, Snapshot.Generation);
    return Sessions.Contains(InInventoryId);
}

bool UInventoryNetworkComponent::RegisterAction(FName InAction, FInventoryClientActionHandler InHandler)
{
    if(InAction.IsNone() || !InHandler || Actions.Contains(InAction) || !GetOwner() || !GetOwner()->HasAuthority())
    {
        return false;
    }
    Actions.Add(InAction, MoveTemp(InHandler));
    return true;
}

void UInventoryNetworkComponent::UnregisterAction(FName InAction)
{
    Actions.Remove(InAction);
}

uint64 UInventoryNetworkComponent::SubmitRequest(FInventoryClientRequest InRequest)
{
    const UInventoryWorldSubsystem* World = WorldSubsystem.Get();
    if(!ConnectionId.IsValid() || !ConnectionStoreId.IsValid() || !WorldEpoch.IsValid() || NextRequestId == MAX_uint64 || Pending.Num() >= 64 ||
       (GetOwner() && GetOwner()->HasAuthority() && World && World->IsArchiveFrozen()))
    {
        return 0;
    }
    InRequest.ConnectionId = ConnectionId;
    InRequest.StoreId = ConnectionStoreId;
    InRequest.WorldEpoch = WorldEpoch;
    InRequest.RequestId = NextRequestId++;
    InRequest.Participants.Reset();
    TSet<FGuid> ParticipantIds;
    if(InRequest.SourceInventoryId.IsValid())
    {
        ParticipantIds.Add(InRequest.SourceInventoryId);
    }
    if(InRequest.TargetInventoryId.IsValid())
    {
        ParticipantIds.Add(InRequest.TargetInventoryId);
    }
    for(const FGuid& InventoryId : InRequest.TargetInventories)
    {
        ParticipantIds.Add(InventoryId);
    }
    if(ParticipantIds.Num() > InventoryNetworkFormat::MaximumParticipants)
    {
        return 0;
    }
    TArray<FGuid> SortedIds = ParticipantIds.Array();
    SortedIds.Sort(InventoryOrder::GuidLess);
    for(const FGuid& InventoryId : SortedIds)
    {
        const FClientView* View = Views.Find(InventoryId);
        if(!View)
        {
            return 0;
        }
        FInventoryClientParticipant Participant;
        Participant.InventoryId = InventoryId;
        Participant.SessionId = View->SessionId;
        Participant.Revision = View->Snapshot.Revision;
        Participant.Generation = View->Snapshot.Generation;
        InRequest.Participants.Add(Participant);
    }
    if(InRequest.SourceSlot.IsValid())
    {
        const FInventorySnapshot* Source = FindView(InRequest.SourceInventoryId);
        const FInventorySlotState* Slot = Source ? Source->FindSlot(InRequest.SourceSlot) : nullptr;
        if(!Slot)
        {
            return 0;
        }
        InRequest.ExpectedSignature = Slot->Item.Signature();
    }
    FPendingRequest Entry;
    Entry.Request = MoveTemp(InRequest);
    Entry.Attempts = 0;
    const uint64 RequestId = Entry.Request.RequestId;
    Pending.Add(RequestId, MoveTemp(Entry));
    return RequestId;
}

void UInventoryNetworkComponent::OpenInventory(const FGuid& InInventoryId, uint32 InGeneration)
{
    if(ConnectionId.IsValid())
    {
        ServerOpen(ConnectionId, ConnectionStoreId, InInventoryId, InGeneration);
    }
}

void UInventoryNetworkComponent::CloseInventory(const FGuid& InInventoryId)
{
    if(InInventoryId == OwnerInventoryId)
    {
        return;
    }
    const FGuid SessionId = GetSessionId(InInventoryId);
    if(SessionId.IsValid())
    {
        ServerClose(SessionId);
    }
}

void UInventoryNetworkComponent::ResyncInventory(const FGuid& InInventoryId)
{
    const FGuid SessionId = GetSessionId(InInventoryId);
    if(SessionId.IsValid())
    {
        ServerResync(InInventoryId, SessionId);
    }
}

const FInventorySnapshot* UInventoryNetworkComponent::FindView(const FGuid& InInventoryId) const
{
    const FClientView* View = Views.Find(InInventoryId);
    return View ? &View->Snapshot : nullptr;
}

FGuid UInventoryNetworkComponent::GetOwnerInventoryId() const
{
    return OwnerInventoryId;
}

FGuid UInventoryNetworkComponent::GetWorldEpoch() const
{
    return WorldEpoch;
}

FGuid UInventoryNetworkComponent::GetStoreId() const
{
    return ConnectionStoreId;
}

FGuid UInventoryNetworkComponent::GetSessionId(const FGuid& InInventoryId) const
{
    const FClientView* View = Views.Find(InInventoryId);
    return View ? View->SessionId : FGuid();
}

bool UInventoryNetworkComponent::ConsumeBudget(double InCost)
{
    const double Now = FPlatformTime::Seconds();
    Tokens = FMath::Min(40.0, Tokens + FMath::Max(0.0, Now - BudgetUpdatedAt) * 20.0);
    BudgetUpdatedAt = Now;
    if(Tokens < InCost)
    {
        return false;
    }
    Tokens -= InCost;
    return true;
}

void UInventoryNetworkComponent::ServerOpen_Implementation(FGuid InConnectionId, FGuid InStoreId, FGuid InInventoryId, uint32 InGeneration)
{
    UInventoryWorldSubsystem* World = WorldSubsystem.Get();
    APlayerController* Controller = Cast<APlayerController>(GetOwner());
    if(!World || !InConnectionId.IsValid() || InConnectionId != ConnectionId || InStoreId != ConnectionStoreId ||
       InStoreId != World->GetStoreId() || WorldEpoch != World->GetWorldEpoch())
    {
        return;
    }
    if(!PlayerId.IsValid() || !ConsumeBudget(4.0))
    {
        ClientClosed(ConnectionId, InInventoryId, FGuid(), EInventoryResultCode::RateLimited);
        return;
    }
    const EInventoryResultCode Access = World->ValidateAccess(Controller, PlayerId, InInventoryId, false);
    if(Access != EInventoryResultCode::Success)
    {
        ClientClosed(ConnectionId, InInventoryId, FGuid(), Access);
        return;
    }
    FInventorySnapshot Snapshot;
    FString Error;
    if(!World->GetSnapshot(InInventoryId, Snapshot, Error) || Snapshot.Generation != InGeneration)
    {
        ClientClosed(ConnectionId, InInventoryId, FGuid(), EInventoryResultCode::GenerationChanged);
        return;
    }
    if(const FServerSession* Existing = Sessions.Find(InInventoryId))
    {
        SendSnapshot(*Existing);
        return;
    }
    if(Sessions.Num() >= InventoryNetworkFormat::MaximumParticipants || !World->AcquireLease(InInventoryId))
    {
        ClientClosed(ConnectionId, InInventoryId, FGuid(), EInventoryResultCode::RateLimited);
        return;
    }
    FServerSession Session;
    Session.InventoryId = InInventoryId;
    Session.SessionId = FGuid::NewGuid();
    Session.Generation = Snapshot.Generation;
    Sessions.Add(InInventoryId, Session);
    SendSnapshot(Session);
}

void UInventoryNetworkComponent::ServerClose_Implementation(FGuid InSessionId)
{
    TArray<FGuid> InventoryIds;
    Sessions.GetKeys(InventoryIds);
    for(const FGuid& InventoryId : InventoryIds)
    {
        const FServerSession* Session = Sessions.Find(InventoryId);
        if(Session && Session->SessionId == InSessionId && InventoryId != OwnerInventoryId)
        {
            CloseServerSession(InventoryId, EInventoryResultCode::Success);
            return;
        }
    }
}

void UInventoryNetworkComponent::ServerResync_Implementation(FGuid InInventoryId, FGuid InSessionId)
{
    UInventoryWorldSubsystem* World = WorldSubsystem.Get();
    const FServerSession* Session = Sessions.Find(InInventoryId);
    if(!World || !Session || Session->SessionId != InSessionId || !ConsumeBudget(4.0))
    {
        return;
    }
    SendSnapshot(*Session);
}

bool UInventoryNetworkComponent::BuildIdentity(const FInventoryClientRequest& InRequest, TArray<uint8>& OutBytes) const
{
    OutBytes.Reset();
    FMemoryWriter Writer(OutBytes, true);
    FInventoryClientRequest Copy = InRequest;
    bool bSuccess = false;
    Copy.NetSerialize(Writer, nullptr, bSuccess);
    return bSuccess && OutBytes.Num() <= 65536;
}

void UInventoryNetworkComponent::ServerSubmit_Implementation(const FInventoryClientRequest& InRequest)
{
    // 旧分支请求在更新限流和请求水位之前拒绝，不能占用新连接的回执编号。
    const UInventoryWorldSubsystem* World = WorldSubsystem.Get();
    if(!World || !InRequest.ConnectionId.IsValid() || InRequest.ConnectionId != ConnectionId ||
       InRequest.StoreId != ConnectionStoreId || InRequest.StoreId != World->GetStoreId() ||
       InRequest.WorldEpoch != WorldEpoch || InRequest.WorldEpoch != World->GetWorldEpoch())
    {
        FInventoryCommandResult Rejected;
        Rejected.RequestId = InRequest.RequestId;
        Rejected.Code = EInventoryResultCode::GenerationChanged;
        ClientResult(InRequest.ConnectionId, Rejected);
        return;
    }
    FInventoryCommandResult Result;
    Result.RequestId = InRequest.RequestId;
    Result.Code = EInventoryResultCode::InvalidRequest;
    TArray<uint8> Identity;
    if(InRequest.RequestId == 0 || !BuildIdentity(InRequest, Identity))
    {
        DeliverResult(Result);
        return;
    }
    if(!ConsumeBudget(1.0))
    {
        Result.Code = EInventoryResultCode::RateLimited;
        DeliverResult(Result);
        return;
    }
    if(InRequest.RequestId <= RequestHighWaterMark)
    {
        for(const FActionReceipt& Receipt : Receipts)
        {
            if(Receipt.RequestId == InRequest.RequestId)
            {
                DeliverResult(Receipt.Identity == Identity ? Receipt.Result : Result);
                return;
            }
        }
        DeliverResult(Result);
        return;
    }
    RequestHighWaterMark = InRequest.RequestId;
    const FGuid RequestConnection = ConnectionId;
    Result = ExecuteRequest(InRequest, Identity);
    if(ConnectionId != RequestConnection)
    {
        return;
    }
    Result.RequestId = InRequest.RequestId;
    Result.ChangedSlots.Reset();
    Result.FinalRevisions.RemoveAll([this](const FInventoryRevision& Revision)
    {
        return !Sessions.Contains(Revision.InventoryId);
    });
    FActionReceipt Receipt;
    Receipt.RequestId = InRequest.RequestId;
    Receipt.Identity = MoveTemp(Identity);
    Receipt.Result = Result;
    Receipts.Add(MoveTemp(Receipt));
    if(Receipts.Num() > 256)
    {
        Receipts.RemoveAt(0, Receipts.Num() - 256, EAllowShrinking::No);
    }
    DeliverResult(Result);
    if(Result.Code == EInventoryResultCode::RevisionStale)
    {
        for(const FInventoryClientParticipant& Participant : InRequest.Participants)
        {
            if(const FServerSession* Session = Sessions.Find(Participant.InventoryId))
            {
                SendSnapshot(*Session);
            }
        }
    }
}

FInventoryCommandResult UInventoryNetworkComponent::ExecuteRequest(const FInventoryClientRequest& InRequest, TConstArrayView<uint8> InIdentity)
{
    FInventoryCommandResult Result;
    Result.RequestId = InRequest.RequestId;
    Result.Code = EInventoryResultCode::InvalidRequest;
    UInventoryWorldSubsystem* World = WorldSubsystem.Get();
    APlayerController* Controller = Cast<APlayerController>(GetOwner());
    if(!World || !World->IsReady() || World->IsArchiveFrozen() || InRequest.ConnectionId != ConnectionId ||
       InRequest.StoreId != ConnectionStoreId || InRequest.StoreId != World->GetStoreId() ||
       InRequest.WorldEpoch != World->GetWorldEpoch() || InRequest.WorldEpoch != WorldEpoch ||
       !PlayerId.IsValid() || InRequest.Quantity < 0 || InRequest.Quantity > 1000000 || InRequest.Participants.Num() > InventoryNetworkFormat::MaximumParticipants)
    {
        Result.Code = EInventoryResultCode::WorldNotReady;
        return Result;
    }
    TArray<FGuid> Allowed;
    FInventoryBatchRequest Batch;
    Batch.RequestId = InRequest.RequestId;
    Batch.RequestIdentity.Append(InIdentity.GetData(), InIdentity.Num());
    for(const FInventoryClientParticipant& Participant : InRequest.Participants)
    {
        const FServerSession* Session = Sessions.Find(Participant.InventoryId);
        if(!Session || Session->SessionId != Participant.SessionId || Session->Generation != Participant.Generation || Allowed.Contains(Participant.InventoryId))
        {
            Result.Code = EInventoryResultCode::GenerationChanged;
            return Result;
        }
        const EInventoryResultCode Access = World->ValidateAccess(Controller, PlayerId, Participant.InventoryId, true);
        if(Access != EInventoryResultCode::Success)
        {
            Result.Code = Access;
            return Result;
        }
        FInventorySnapshot Snapshot;
        FString Error;
        if(!World->GetSnapshot(Participant.InventoryId, Snapshot, Error) || Snapshot.Revision != Participant.Revision)
        {
            Result.Code = EInventoryResultCode::RevisionStale;
            return Result;
        }
        Allowed.Add(Participant.InventoryId);
        Batch.ExpectedRevisions.Add(FInventoryExpectedRevision{Participant.InventoryId, Participant.Revision, Participant.Generation});
    }
    if((InRequest.SourceInventoryId.IsValid() && !Allowed.Contains(InRequest.SourceInventoryId)) ||
       (InRequest.TargetInventoryId.IsValid() && !Allowed.Contains(InRequest.TargetInventoryId)))
    {
        Result.Code = EInventoryResultCode::Unauthorized;
        return Result;
    }
    for(const FGuid& TargetId : InRequest.TargetInventories)
    {
        if(!Allowed.Contains(TargetId))
        {
            Result.Code = EInventoryResultCode::Unauthorized;
            return Result;
        }
    }
    if(InRequest.SourceSlot.IsValid())
    {
        FInventorySnapshot Source;
        FString Error;
        if(!World->GetSnapshot(InRequest.SourceInventoryId, Source, Error))
        {
            Result.Code = EInventoryResultCode::InventoryGone;
            return Result;
        }
        const FInventorySlotState* Slot = Source.FindSlot(InRequest.SourceSlot);
        if(!Slot || Slot->Item.Signature() != InRequest.ExpectedSignature)
        {
            Result.Code = EInventoryResultCode::RevisionStale;
            return Result;
        }
    }
    const FInventoryAuthorityContext Authority = World->MakeClientAuthority(Controller, PlayerId, ConnectionId, Allowed);
    if(!InRequest.DomainAction.IsNone())
    {
        const FInventoryClientActionHandler* Handler = Actions.Find(InRequest.DomainAction);
        if(!Handler || InRequest.SourceInventoryId != OwnerInventoryId || !Allowed.Contains(OwnerInventoryId))
        {
            Result.Code = EInventoryResultCode::Unauthorized;
            return Result;
        }
        const FInventoryClientActionHandler Callback = *Handler;
        return Callback(InRequest, Authority);
    }
    if(!InventoryNetworkFormat::IsClientOperation(InRequest.Operation))
    {
        Result.Code = EInventoryResultCode::Unauthorized;
        return Result;
    }
    FInventoryOperation Operation;
    Operation.Type = InRequest.Operation;
    Operation.AmountMode = InRequest.bAllowPartial ? EInventoryAmountMode::UpTo : EInventoryAmountMode::Exact;
    Operation.SourceInventoryId = InRequest.SourceInventoryId;
    Operation.TargetInventoryId = InRequest.TargetInventoryId;
    Operation.TargetInventories = InRequest.TargetInventories;
    Operation.SourceSlot = InRequest.SourceSlot;
    Operation.TargetSlot = InRequest.TargetSlot;
    if(InRequest.SourceGroup.IsValid())
    {
        Operation.SourceGroups.Add(InRequest.SourceGroup);
    }
    if(InRequest.TargetGroup.IsValid())
    {
        Operation.TargetGroups.Add(InRequest.TargetGroup);
    }
    Operation.Requested = InRequest.Quantity;
    Operation.bFavorite = InRequest.bFavorite;
    Operation.bAutomatic = InRequest.bAutomatic || InRequest.Operation == EInventoryOperationType::QuickStack || InRequest.Operation == EInventoryOperationType::Sort;
    Operation.Filter.bMatchSignature = InRequest.SourceSlot.IsValid();
    Operation.Filter.Signature = InRequest.ExpectedSignature;
    Batch.Operations.Add(MoveTemp(Operation));
    return World->ExecuteAuthorizedBatch(Batch, Authority);
}

void UInventoryNetworkComponent::DeliverResult(FInventoryCommandResult InResult)
{
    InResult.ChangedSlots.Reset();
    InResult.FinalRevisions.RemoveAll([this](const FInventoryRevision& Revision)
    {
        return !Sessions.Contains(Revision.InventoryId);
    });
    ClientResult(ConnectionId, InResult);
}

void UInventoryNetworkComponent::SendSnapshot(const FServerSession& InSession)
{
    const FServerSession Session = InSession;
    UInventoryWorldSubsystem* World = WorldSubsystem.Get();
    if(!World)
    {
        return;
    }
    const EInventoryResultCode Access = World->ValidateAccess(Cast<APlayerController>(GetOwner()), PlayerId, Session.InventoryId, false);
    const FServerSession* Current = Sessions.Find(Session.InventoryId);
    if(!Current || Current->SessionId != Session.SessionId)
    {
        return;
    }
    if(Access != EInventoryResultCode::Success)
    {
        CloseServerSession(Session.InventoryId, Access);
        return;
    }
    FInventorySnapshot Snapshot;
    TArray<uint8> Bytes;
    FString Error;
    if(!World->GetSnapshot(Session.InventoryId, Snapshot, Error) || Snapshot.Generation != Session.Generation ||
       !FInventorySnapshotCodec::Encode(Snapshot, World->GetRegistry(), World->GetProfiles(), Bytes, Error))
    {
        CloseServerSession(Session.InventoryId, EInventoryResultCode::InvalidState);
        return;
    }
    FInventorySnapshotChunk Chunk;
    Chunk.ConnectionId = ConnectionId;
    Chunk.StoreId = ConnectionStoreId;
    Chunk.WorldEpoch = WorldEpoch;
    Chunk.SessionId = Session.SessionId;
    Chunk.InventoryId = Session.InventoryId;
    Chunk.TransferId = FGuid::NewGuid();
    Chunk.Revision = Snapshot.Revision;
    Chunk.Generation = Snapshot.Generation;
    Chunk.TotalBytes = Bytes.Num();
    Chunk.Count = FMath::DivideAndRoundUp(Bytes.Num(), InventoryNetworkFormat::ChunkBytes);
    Chunk.Checksum = FCrc::MemCrc32(Bytes.GetData(), Bytes.Num());
    for(int32 ChunkIndex = 0; ChunkIndex < Chunk.Count; ++ChunkIndex)
    {
        Chunk.Index = ChunkIndex;
        const int32 Offset = ChunkIndex * InventoryNetworkFormat::ChunkBytes;
        const int32 Count = FMath::Min(InventoryNetworkFormat::ChunkBytes, Bytes.Num() - Offset);
        Chunk.Bytes.Reset(Count);
        Chunk.Bytes.Append(Bytes.GetData() + Offset, Count);
        ClientSnapshot(Chunk);
    }
}

void UInventoryNetworkComponent::CloseServerSession(const FGuid& InInventoryId, EInventoryResultCode InReason)
{
    const FGuid InventoryId = InInventoryId;
    const FServerSession* Existing = Sessions.Find(InventoryId);
    if(!Existing)
    {
        return;
    }
    const FGuid SessionId = Existing->SessionId;
    Sessions.Remove(InventoryId);
    if(UInventoryWorldSubsystem* World = WorldSubsystem.Get())
    {
        World->ReleaseLease(InventoryId);
    }
    ClientClosed(ConnectionId, InventoryId, SessionId, InReason);
}

void UInventoryNetworkComponent::HandleCommit(const FInventoryChangeSet& InChanges)
{
    UInventoryWorldSubsystem* World = WorldSubsystem.Get();
    if(!World)
    {
        return;
    }
    for(const FInventoryRevision& Revision : InChanges.Revisions)
    {
        const FServerSession* Session = Sessions.Find(Revision.InventoryId);
        if(!Session)
        {
            continue;
        }
        const FServerSession CommittedSession = *Session;
        const EInventoryResultCode Access = World->ValidateAccess(Cast<APlayerController>(GetOwner()), PlayerId, Revision.InventoryId, false);
        const FServerSession* CurrentSession = Sessions.Find(Revision.InventoryId);
        if(!CurrentSession || CurrentSession->SessionId != CommittedSession.SessionId)
        {
            continue;
        }
        if(Access != EInventoryResultCode::Success)
        {
            CloseServerSession(Revision.InventoryId, Access);
            continue;
        }
        FInventoryDelta Delta;
        Delta.ConnectionId = ConnectionId;
        Delta.StoreId = ConnectionStoreId;
        Delta.WorldEpoch = WorldEpoch;
        Delta.SessionId = CommittedSession.SessionId;
        Delta.InventoryId = Revision.InventoryId;
        Delta.Generation = CommittedSession.Generation;
        Delta.TransactionId = InChanges.TransactionId;
        Delta.FromRevision = Revision.Before;
        Delta.ToRevision = Revision.After;
        for(const FInventoryChangedSlot& Slot : InChanges.ChangedSlots)
        {
            if(Slot.InventoryId == Revision.InventoryId)
            {
                Delta.Slots.Add(Slot.After);
            }
        }
        if(Delta.Slots.Num() > 128)
        {
            SendSnapshot(CommittedSession);
        }
        else
        {
            ClientDelta(Delta);
        }
    }
}

void UInventoryNetworkComponent::HandleDeleted(const FGuid& InInventoryId)
{
    CloseServerSession(InInventoryId, EInventoryResultCode::InventoryGone);
}

void UInventoryNetworkComponent::UnbindOwner()
{
    if(bResettingConnection)
    {
        return;
    }
    TGuardValue<bool> ResetGuard(bResettingConnection, true);
    const FGuid OldConnection = ConnectionId;
    UInventoryWorldSubsystem* World = WorldSubsystem.Get();
    const EInventoryResultCode Reason = World && World->IsQuarantined() ? EInventoryResultCode::ConfirmationPending : EInventoryResultCode::WorldNotReady;
    TArray<FGuid> InventoryIds;
    Sessions.GetKeys(InventoryIds);
    for(const FGuid& InventoryId : InventoryIds)
    {
        CloseServerSession(InventoryId, Reason);
    }
    if(World && World->Transactions && OldConnection.IsValid())
    {
        World->Transactions->ForgetConnection(OldConnection);
    }
    if(GetOwner() && GetOwner()->HasAuthority())
    {
        ClientReset(OldConnection, Reason);
    }
    else
    {
        ClientReset_Implementation(OldConnection, Reason);
    }
    PlayerId.Invalidate();
    OwnerInventoryId.Invalidate();
    ConnectionId.Invalidate();
    WorldEpoch.Invalidate();
    ConnectionWorldId.Invalidate();
    ConnectionStoreId.Invalidate();
    Receipts.Reset();
    RequestHighWaterMark = 0;
    NextRequestId = 1;
    SessionCheckAccumulator = 0;
}

void UInventoryNetworkComponent::HandleClosing()
{
    UnbindOwner();
}

void UInventoryNetworkComponent::HandleStoreAdopted()
{
    if(!GetOwner() || !GetOwner()->HasAuthority() || !PlayerId.IsValid() || !OwnerInventoryId.IsValid()) return;
    const FGuid BoundPlayer = PlayerId;
    const FGuid BoundInventory = OwnerInventoryId;
    if(!BindOwner(BoundPlayer, BoundInventory)) UnbindOwner();
}

void UInventoryNetworkComponent::ClientReset_Implementation(FGuid InConnectionId, EInventoryResultCode InReason)
{
    if(InConnectionId != ConnectionId)
    {
        return;
    }
    TArray<FGuid> Existing;
    Views.GetKeys(Existing);
    TMap<uint64, FPendingRequest> Cancelled = MoveTemp(Pending);
    Pending.Reset();
    Views.Reset();
    Assemblies.Reset();
    ConnectionId.Invalidate();
    ConnectionWorldId.Invalidate();
    ConnectionStoreId.Invalidate();
    WorldEpoch.Invalidate();
    OwnerInventoryId.Invalidate();
    NextRequestId = 1;
    for(const TPair<uint64, FPendingRequest>& Pair : Cancelled)
    {
        FInventoryCommandResult Result;
        Result.RequestId = Pair.Key;
        Result.Code = Pair.Value.Attempts > 0 ? EInventoryResultCode::ConfirmationPending : InReason;
        OnResult.Broadcast(Result);
    }
    for(const FGuid& InventoryId : Existing)
    {
        OnSessionClosed.Broadcast(InventoryId, InReason);
    }
}

void UInventoryNetworkComponent::ClientBegin_Implementation(FGuid InConnectionId, FGuid InWorldId, FGuid InStoreId, FGuid InWorldEpoch, FGuid InOwnerInventoryId)
{
    ClientReset_Implementation(ConnectionId, EInventoryResultCode::GenerationChanged);
    ConnectionId = InConnectionId;
    ConnectionWorldId = InWorldId;
    ConnectionStoreId = InStoreId;
    WorldEpoch = InWorldEpoch;
    OwnerInventoryId = InOwnerInventoryId;
    NextRequestId = 1;
}

void UInventoryNetworkComponent::ClientSnapshot_Implementation(const FInventorySnapshotChunk& InChunk)
{
    UInventoryWorldSubsystem* World = WorldSubsystem.Get();
    if(!World || !ConnectionId.IsValid() || InChunk.ConnectionId != ConnectionId || InChunk.StoreId != ConnectionStoreId ||
       InChunk.WorldEpoch != WorldEpoch || !InChunk.SessionId.IsValid() || !InChunk.TransferId.IsValid() ||
       InChunk.TotalBytes < 1 || InChunk.TotalBytes > FInventorySnapshotCodec::MaximumBytes || InChunk.Count < 1 ||
       InChunk.Count != FMath::DivideAndRoundUp(InChunk.TotalBytes, InventoryNetworkFormat::ChunkBytes) ||
       InChunk.Index < 0 || InChunk.Index >= InChunk.Count)
    {
        return;
    }
    const int32 Offset = InChunk.Index * InventoryNetworkFormat::ChunkBytes;
    const int32 Expected = FMath::Min(InventoryNetworkFormat::ChunkBytes, InChunk.TotalBytes - Offset);
    if(InChunk.Bytes.Num() != Expected)
    {
        return;
    }
    if(const FClientView* View = Views.Find(InChunk.InventoryId))
    {
        if(View->SessionId == InChunk.SessionId && View->Snapshot.Revision > InChunk.Revision)
        {
            return;
        }
    }
    FAssembly* Assembly = Assemblies.Find(InChunk.TransferId);
    if(!Assembly)
    {
        TArray<FGuid> Replaced;
        for(const TPair<FGuid, FAssembly>& Pair : Assemblies)
        {
            if(Pair.Value.Header.InventoryId == InChunk.InventoryId)
            {
                Replaced.Add(Pair.Key);
            }
        }
        for(const FGuid& TransferId : Replaced)
        {
            Assemblies.Remove(TransferId);
        }
        if(Assemblies.Num() >= InventoryNetworkFormat::MaximumParticipants)
        {
            return;
        }
        FAssembly NewAssembly;
        NewAssembly.Header = InChunk;
        NewAssembly.Header.Bytes.Reset();
        NewAssembly.Bytes.SetNumUninitialized(InChunk.TotalBytes);
        NewAssembly.Received.Init(false, InChunk.Count);
        NewAssembly.StartedAt = FPlatformTime::Seconds();
        Assembly = &Assemblies.Add(InChunk.TransferId, MoveTemp(NewAssembly));
    }
    const FInventorySnapshotChunk& Header = Assembly->Header;
    if(Header.ConnectionId != InChunk.ConnectionId || Header.StoreId != InChunk.StoreId ||
       Header.SessionId != InChunk.SessionId || Header.InventoryId != InChunk.InventoryId || Header.Revision != InChunk.Revision ||
       Header.Generation != InChunk.Generation || Header.Count != InChunk.Count || Header.TotalBytes != InChunk.TotalBytes || Header.Checksum != InChunk.Checksum)
    {
        Assemblies.Remove(InChunk.TransferId);
        return;
    }
    if(!Assembly->Received[InChunk.Index])
    {
        FMemory::Memcpy(Assembly->Bytes.GetData() + Offset, InChunk.Bytes.GetData(), InChunk.Bytes.Num());
        Assembly->Received[InChunk.Index] = true;
        ++Assembly->ReceivedCount;
    }
    if(Assembly->ReceivedCount != InChunk.Count)
    {
        return;
    }
    FInventorySnapshot Snapshot;
    FString Error;
    const bool bDecoded = FCrc::MemCrc32(Assembly->Bytes.GetData(), Assembly->Bytes.Num()) == InChunk.Checksum &&
        FInventorySnapshotCodec::Decode(Assembly->Bytes, World->GetRegistry(), World->GetProfiles(), Snapshot, Error) &&
        Snapshot.WorldId == ConnectionWorldId && Snapshot.InventoryId == InChunk.InventoryId && Snapshot.Revision == InChunk.Revision &&
        Snapshot.Generation == InChunk.Generation;
    Assemblies.Remove(InChunk.TransferId);
    if(!bDecoded)
    {
        ClientClosed_Implementation(InChunk.ConnectionId, InChunk.InventoryId, InChunk.SessionId, EInventoryResultCode::InvalidItem);
        return;
    }
    FClientView View;
    View.SessionId = InChunk.SessionId;
    View.Snapshot = MoveTemp(Snapshot);
    Views.Add(InChunk.InventoryId, MoveTemp(View));
    const FInventorySnapshot Published = Views.FindChecked(InChunk.InventoryId).Snapshot;
    OnSnapshot.Broadcast(Published);
}

void UInventoryNetworkComponent::ClientDelta_Implementation(const FInventoryDelta& InDelta)
{
    if(!ConnectionId.IsValid() || InDelta.ConnectionId != ConnectionId || InDelta.StoreId != ConnectionStoreId || InDelta.WorldEpoch != WorldEpoch)
    {
        return;
    }
    UInventoryWorldSubsystem* World = WorldSubsystem.Get();
    FClientView* View = Views.Find(InDelta.InventoryId);
    if(!World || !View || View->SessionId != InDelta.SessionId || View->Snapshot.Generation != InDelta.Generation)
    {
        ServerResync(InDelta.InventoryId, InDelta.SessionId);
        return;
    }
    if(InDelta.ToRevision <= View->Snapshot.Revision)
    {
        return;
    }
    if(InDelta.FromRevision != View->Snapshot.Revision || InDelta.FromRevision == MAX_uint64 ||
       InDelta.ToRevision != InDelta.FromRevision + 1 || InDelta.Slots.Num() > 128)
    {
        ServerResync(InDelta.InventoryId, InDelta.SessionId);
        return;
    }
    FInventorySnapshot Snapshot = View->Snapshot;
    TSet<FInventorySlotKey> Keys;
    for(const FInventorySlotState& Changed : InDelta.Slots)
    {
        FInventorySlotState* Target = Snapshot.Slots.FindByPredicate([&Changed](const FInventorySlotState& Candidate)
        {
            return Candidate.Key == Changed.Key;
        });
        if(!Target || Keys.Contains(Changed.Key))
        {
            ServerResync(InDelta.InventoryId, InDelta.SessionId);
            return;
        }
        Keys.Add(Changed.Key);
        *Target = Changed;
    }
    Snapshot.Revision = InDelta.ToRevision;
    FString Error;
    const FInventoryProfileDefinition* Profile = World->GetProfiles().Find(Snapshot.ProfileId);
    if(!Profile || !Profile->ValidateState(Snapshot, World->GetRegistry(), Error))
    {
        ServerResync(InDelta.InventoryId, InDelta.SessionId);
        return;
    }
    View->Snapshot = MoveTemp(Snapshot);
    OnDelta.Broadcast(InDelta);
}

void UInventoryNetworkComponent::ClientResult_Implementation(FGuid InConnectionId, const FInventoryCommandResult& InResult)
{
    if(InConnectionId == ConnectionId && Pending.Remove(InResult.RequestId) > 0)
    {
        OnResult.Broadcast(InResult);
    }
}

void UInventoryNetworkComponent::ClientClosed_Implementation(FGuid InConnectionId, FGuid InInventoryId, FGuid InSessionId, EInventoryResultCode InReason)
{
    if(!ConnectionId.IsValid() || InConnectionId != ConnectionId) return;
    const FClientView* View = Views.Find(InInventoryId);
    if(View && (!InSessionId.IsValid() || View->SessionId != InSessionId))
    {
        return;
    }
    Views.Remove(InInventoryId);
    TArray<FGuid> RemovedAssemblies;
    for(const TPair<FGuid, FAssembly>& Pair : Assemblies)
    {
        if(Pair.Value.Header.InventoryId == InInventoryId)
        {
            RemovedAssemblies.Add(Pair.Key);
        }
    }
    for(const FGuid& TransferId : RemovedAssemblies)
    {
        Assemblies.Remove(TransferId);
    }
    TArray<uint64> Cancelled;
    for(const TPair<uint64, FPendingRequest>& Pair : Pending)
    {
        if(Pair.Value.Request.Participants.ContainsByPredicate([&InInventoryId](const FInventoryClientParticipant& Participant)
        {
            return Participant.InventoryId == InInventoryId;
        }))
        {
            Cancelled.Add(Pair.Key);
        }
    }
    for(uint64 RequestId : Cancelled)
    {
        const FPendingRequest* Request = Pending.Find(RequestId);
        const bool bSent = Request && Request->Attempts > 0;
        Pending.Remove(RequestId);
        FInventoryCommandResult Result;
        Result.RequestId = RequestId;
        Result.Code = bSent ? EInventoryResultCode::ConfirmationPending : InReason;
        OnResult.Broadcast(Result);
    }
    OnSessionClosed.Broadcast(InInventoryId, InReason);
}
