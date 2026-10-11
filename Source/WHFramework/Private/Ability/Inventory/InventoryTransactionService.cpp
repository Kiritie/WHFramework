#include "Ability/Inventory/InventoryTransactionService.h"

#include "Ability/Inventory/InventoryCommitCoordinator.h"
#include "Ability/Inventory/InventoryWorldSubsystem.h"
#include "Ability/Inventory/InventorySnapshotCodec.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeExit.h"

FInventoryTransactionService::FInventoryTransactionService(UInventoryWorldSubsystem& InWorld)
    : World(InWorld)
{
}

FInventoryCommandResult FInventoryTransactionService::Execute(const FInventoryBatchRequest& InRequest, const FInventoryAuthorityContext& InAuthority)
{
    FInventoryCommandResult Result;
    Result.RequestId = InRequest.RequestId;
    if(!IsInGameThread() || !World.IsReady() || World.IsArchiveFrozen() || World.bCommitting || World.bPublishing ||
       !InAuthority.ActorId.IsValid() || InAuthority.WorldEpoch != World.WorldEpoch || InAuthority.Reason.IsNone())
    {
        Result.Code = EInventoryResultCode::InvalidState;
        return Result;
    }
    if(InAuthority.StoreId != World.StoreId)
    {
        Result.Code = EInventoryResultCode::GenerationChanged;
        return Result;
    }
    FConnectionReceipts* Receipts = nullptr;
    if(!InAuthority.bServerDomain)
    {
        if(!InAuthority.ConnectionId.IsValid() || !InAuthority.Controller.IsValid() ||
           InRequest.RequestId == 0 || InRequest.RequestIdentity.IsEmpty() || InRequest.RequestIdentity.Num() > 65536)
        {
            Result.Code = EInventoryResultCode::Unauthorized;
            return Result;
        }
        Receipts = Connections.Find(InAuthority.ConnectionId);
        if(!Receipts)
        {
            if(Connections.Num() >= 4096)
            {
                Result.Code = EInventoryResultCode::RateLimited;
                return Result;
            }
            FConnectionReceipts NewReceipts;
            NewReceipts.ActorId = InAuthority.ActorId;
            Receipts = &Connections.Add(InAuthority.ConnectionId, MoveTemp(NewReceipts));
        }
        if(Receipts->ActorId != InAuthority.ActorId)
        {
            Result.Code = EInventoryResultCode::Unauthorized;
            return Result;
        }
        if(InRequest.RequestId <= Receipts->HighWaterMark)
        {
            for(const FReceipt& Receipt : Receipts->Recent)
            {
                if(Receipt.RequestId == InRequest.RequestId)
                {
                    if(Receipt.Identity == InRequest.RequestIdentity)
                    {
                        return Receipt.Result;
                    }
                    Result.Code = EInventoryResultCode::InvalidRequest;
                    return Result;
                }
            }
            Result.Code = EInventoryResultCode::InvalidRequest;
            return Result;
        }
        Receipts->HighWaterMark = InRequest.RequestId;
    }
    const double StartedAt = FPlatformTime::Seconds();
    Result = ExecuteNew(InRequest, InAuthority);
    Result.RequestId = InRequest.RequestId;
    Receipts = InAuthority.bServerDomain ? nullptr : Connections.Find(InAuthority.ConnectionId);
    if(Receipts)
    {
        FReceipt Receipt;
        Receipt.RequestId = InRequest.RequestId;
        Receipt.Identity = InRequest.RequestIdentity;
        Receipt.Result = Result;
        Receipts->Recent.Add(MoveTemp(Receipt));
        if(Receipts->Recent.Num() > 256)
        {
            Receipts->Recent.RemoveAt(0, Receipts->Recent.Num() - 256, EAllowShrinking::No);
        }
    }
    UE_LOG(LogTemp, Verbose, TEXT("Inventory World=%s Store=%s Tx=%s Actor=%s Ops=%d Code=%d Accepted=%lld Remaining=%lld DurationMs=%.3f"),
        *World.GetWorldId().ToString(), *World.GetStoreId().ToString(), *Result.TransactionId.ToString(), *InAuthority.ActorId.ToString(), InRequest.Operations.Num(),
        static_cast<int32>(Result.Code), static_cast<long long>(Result.AcceptedCount), static_cast<long long>(Result.RemainingCount),
        (FPlatformTime::Seconds() - StartedAt) * 1000.0);
    return Result;
}

FInventoryCommandResult FInventoryTransactionService::ExecuteNew(const FInventoryBatchRequest& InRequest, const FInventoryAuthorityContext& InAuthority)
{
    FInventoryCommandResult Result;
    Result.RequestId = InRequest.RequestId;
    Result.Code = EInventoryResultCode::InvalidRequest;
    const int32 MaximumOperations = InAuthority.bServerDomain ? 4096 : 256;
    if(InRequest.Operations.Num() > MaximumOperations || InRequest.CreateInventories.Num() > 32 || InRequest.DeleteInventories.Num() > 32 ||
       InRequest.DomainChanges.Num() > 256 || InRequest.ExpectedRevisions.Num() > 64)
    {
        return Result;
    }
    if(!InAuthority.bServerDomain && (!InRequest.CreateInventories.IsEmpty() || !InRequest.DeleteInventories.IsEmpty() || !InRequest.DomainChanges.IsEmpty()))
    {
        Result.Code = EInventoryResultCode::Unauthorized;
        return Result;
    }
    FString Error;
    if(World.Journal.NeedsCheckpoint() && !World.SaveCheckpoint(Error))
    {
        Result.Code = EInventoryResultCode::SaveUnavailable;
        return Result;
    }
    TSet<FGuid> ParticipantSet;
    for(const FInventoryOperation& Operation : InRequest.Operations)
    {
        if(Operation.SourceGroups.Num() > 16 || Operation.TargetGroups.Num() > 16 || Operation.TargetInventories.Num() > 16 ||
           Operation.Item.Properties.Num() > FInventorySnapshotCodec::MaximumProperties || Operation.ExchangeLines.Num() > 64)
        {
            return Result;
        }
        if(!InAuthority.bServerDomain && (Operation.Type == EInventoryOperationType::Add || Operation.Type == EInventoryOperationType::Remove ||
           Operation.Type == EInventoryOperationType::Transform || Operation.Type == EInventoryOperationType::Exchange))
        {
            Result.Code = EInventoryResultCode::Unauthorized;
            return Result;
        }
        if(Operation.SourceInventoryId.IsValid())
        {
            ParticipantSet.Add(Operation.SourceInventoryId);
        }
        if(Operation.TargetInventoryId.IsValid())
        {
            ParticipantSet.Add(Operation.TargetInventoryId);
        }
        for(const FGuid& InventoryId : Operation.TargetInventories)
        {
            ParticipantSet.Add(InventoryId);
        }
        for(const FInventoryExchangeLine& Line : Operation.ExchangeLines)
        {
            ParticipantSet.Add(Line.SourceInventoryId);
            ParticipantSet.Add(Line.TargetInventoryId);
        }
    }
    TMap<FGuid, FInventorySnapshot> Created;
    for(const FInventorySnapshot& Snapshot : InRequest.CreateInventories)
    {
        if(!Snapshot.InventoryId.IsValid() || Snapshot.WorldId != World.WorldId || Snapshot.Revision != 0 ||
           Created.Contains(Snapshot.InventoryId) || World.Active.Contains(Snapshot.InventoryId) || World.Cold.Contains(Snapshot.InventoryId) ||
           World.Tombstones.Contains(Snapshot.InventoryId))
        {
            return Result;
        }
        const FInventoryProfileDefinition* Profile = World.Profiles.Find(Snapshot.ProfileId);
        if(!Profile || !Profile->ValidateState(Snapshot, World.Registry, Error))
        {
            Result.Code = EInventoryResultCode::InvalidState;
            return Result;
        }
        for(const FInventorySlotState& Slot : Snapshot.Slots)
        {
            if(!Slot.Item.IsEmpty() || Slot.bFavorite)
            {
                Result.Code = EInventoryResultCode::Unauthorized;
                return Result;
            }
        }
        Created.Add(Snapshot.InventoryId, Snapshot);
        ParticipantSet.Add(Snapshot.InventoryId);
    }
    TSet<FGuid> Deleted;
    for(const FGuid& InventoryId : InRequest.DeleteInventories)
    {
        if(!InventoryId.IsValid() || Deleted.Contains(InventoryId) || Created.Contains(InventoryId))
        {
            return Result;
        }
        Deleted.Add(InventoryId);
        ParticipantSet.Add(InventoryId);
    }
    TMap<FGuid, FInventoryExpectedRevision> Expected;
    for(const FInventoryExpectedRevision& Revision : InRequest.ExpectedRevisions)
    {
        if(!Revision.InventoryId.IsValid() || Revision.Generation == 0 || Expected.Contains(Revision.InventoryId))
        {
            return Result;
        }
        ParticipantSet.Add(Revision.InventoryId);
        Expected.Add(Revision.InventoryId, Revision);
    }
    if(ParticipantSet.Num() > 64)
    {
        return Result;
    }
    TArray<FGuid> Participants = ParticipantSet.Array();
    Participants.Sort(InventoryOrder::GuidLess);
    TArray<FGuid> Leases;
    ON_SCOPE_EXIT
    {
        for(const FGuid& InventoryId : Leases)
        {
            World.ReleaseLease(InventoryId);
        }
        World.bCommitting = false;
    };
    TArray<FInventorySnapshot> States;
    World.bCommitting = true;
    for(const FGuid& InventoryId : Participants)
    {
        if(const FInventorySnapshot* NewSnapshot = Created.Find(InventoryId))
        {
            if(const FInventoryExpectedRevision* Revision = Expected.Find(InventoryId); Revision &&
                (Revision->Generation != NewSnapshot->Generation || Revision->Revision != NewSnapshot->Revision))
            {
                Result.Code = EInventoryResultCode::RevisionStale;
                return Result;
            }
            States.Add(*NewSnapshot);
            continue;
        }
        if(!InAuthority.bServerDomain)
        {
            if(!InAuthority.AllowedInventories.Contains(InventoryId))
            {
                Result.Code = EInventoryResultCode::Unauthorized;
                return Result;
            }
            const EInventoryResultCode Access = World.ValidateAccess(InAuthority.Controller.Get(), InAuthority.ActorId, InventoryId, true);
            if(Access != EInventoryResultCode::Success)
            {
                Result.Code = Access;
                return Result;
            }
        }
        FInventorySnapshot Snapshot;
        if(!World.GetSnapshot(InventoryId, Snapshot, Error) || !World.AcquireLease(InventoryId))
        {
            Result.Code = EInventoryResultCode::InventoryGone;
            return Result;
        }
        Leases.Add(InventoryId);
        const FInventoryExpectedRevision* Revision = Expected.Find(InventoryId);
        if(!InAuthority.bServerDomain && !Revision)
        {
            return Result;
        }
        if(Revision && Revision->Generation != Snapshot.Generation)
        {
            Result.Code = EInventoryResultCode::GenerationChanged;
            return Result;
        }
        if(Revision && Revision->Revision != Snapshot.Revision)
        {
            Result.Code = EInventoryResultCode::RevisionStale;
            return Result;
        }
        States.Add(MoveTemp(Snapshot));
    }
    World.bCommitting = true;
    FInventoryPlan Plan;
    const FInventoryPlannerContext Context(World.Registry, World.Profiles, World.Rules.Get());
    Plan.Code = EInventoryResultCode::NoChange;
    if(!InRequest.Operations.IsEmpty())
    {
        const EInventoryResultCode Planned = FInventoryPlanner::PlanBatch(Context, States, InRequest.Operations, Plan, Error);
        if(Planned != EInventoryResultCode::Success && Planned != EInventoryResultCode::Partial && Planned != EInventoryResultCode::NoChange)
        {
            Result.Code = Planned;
            return Result;
        }
    }
    for(const FInventorySnapshot& Snapshot : States)
    {
        FInventorySnapshot* After = Plan.After.FindByPredicate([&Snapshot](const FInventorySnapshot& Candidate)
        {
            return Candidate.InventoryId == Snapshot.InventoryId;
        });
        if(Created.Contains(Snapshot.InventoryId) || Deleted.Contains(Snapshot.InventoryId))
        {
            if(!After)
            {
                if(Snapshot.Revision == MAX_uint64)
                {
                    Result.Code = EInventoryResultCode::InvalidState;
                    return Result;
                }
                Plan.Before.Add(Snapshot);
                FInventorySnapshot Changed = Snapshot;
                Changed.Revision = Snapshot.Revision + 1;
                Plan.After.Add(MoveTemp(Changed));
                After = &Plan.After.Last();
                FInventoryRevision Revision;
                Revision.InventoryId = Snapshot.InventoryId;
                Revision.Before = Snapshot.Revision;
                Revision.After = After->Revision;
                Plan.Changes.Revisions.Add(Revision);
            }
            if(Deleted.Contains(Snapshot.InventoryId))
            {
                for(const FInventorySlotState& Slot : After->Slots)
                {
                    if(!Slot.Item.IsEmpty())
                    {
                        Result.Code = EInventoryResultCode::InvalidState;
                        return Result;
                    }
                }
            }
        }
    }
    if(!World.ValidateInstances(Plan.Before, Plan.After, Error))
    {
        Result.Code = EInventoryResultCode::DuplicateInstance;
        return Result;
    }
    FInventoryBatchRequest PreparedRequest = InRequest;
    TArray<FName> ProjectorNames;
    World.SnapshotProjectors.GetKeys(ProjectorNames);
    ProjectorNames.Sort(FNameLexicalLess());
    for(FName Name : ProjectorNames)
    {
        if(!World.SnapshotProjectors.FindChecked(Name)(Plan, PreparedRequest.DomainChanges, Error))
        {
            Result.Code = EInventoryResultCode::SourcePreparationFailed;
            return Result;
        }
    }
    if(PreparedRequest.DomainChanges.Num() > 256)
    {
        return Result;
    }
    TSet<FInventoryRecordKey> DomainKeys;
    for(const FInventoryDomainRedo& Redo : PreparedRequest.DomainChanges)
    {
        const FInventoryRecordKey Key{Redo.Domain, Redo.EntityId};
        if(DomainKeys.Contains(Key) || !World.Records.ValidateRedo(Redo, Error))
        {
            Result.Code = EInventoryResultCode::RevisionStale;
            return Result;
        }
        DomainKeys.Add(Key);
    }
    if(Plan.After.IsEmpty() && PreparedRequest.DomainChanges.IsEmpty())
    {
        Result.Code = EInventoryResultCode::NoChange;
        return Result;
    }
    if(Plan.Code == EInventoryResultCode::NoChange)
    {
        Plan.Code = EInventoryResultCode::Success;
    }
    FInventoryCommitCoordinator Coordinator(World);
    return Coordinator.Commit(MoveTemp(Plan), PreparedRequest, Error);
}

void FInventoryTransactionService::ForgetConnection(const FGuid& InConnectionId)
{
    Connections.Remove(InConnectionId);
}
