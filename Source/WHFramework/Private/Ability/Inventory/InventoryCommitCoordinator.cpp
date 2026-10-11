#include "Ability/Inventory/InventoryCommitCoordinator.h"

#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/InventoryWorldSubsystem.h"
#include "Ability/Inventory/InventorySnapshotCodec.h"
#include "Misc/ScopeExit.h"

FInventoryCommitCoordinator::FInventoryCommitCoordinator(UInventoryWorldSubsystem& InWorld)
    : World(InWorld)
{
}

bool FInventoryCommitCoordinator::ApplyInventory(const FInventoryJournalChange& InChange, FString& OutError)
{
    if(InChange.bDeleted)
    {
        if(const FInventoryTombstone* Tombstone = World.Tombstones.Find(InChange.InventoryId))
        {
            if(Tombstone->Revision == InChange.AfterRevision && Tombstone->Generation == InChange.Generation)
            {
                return true;
            }
            OutError = TEXT("Inventory.TombstoneDiverged");
            return false;
        }
        FInventorySnapshot Before;
        if(!World.GetSnapshot(InChange.InventoryId, Before, OutError) || Before.Revision != InChange.BeforeRevision || Before.Generation != InChange.Generation)
        {
            OutError = TEXT("Inventory.DeleteReplayDiverged");
            return false;
        }
        World.Active.Remove(InChange.InventoryId);
        World.Cold.Remove(InChange.InventoryId);
        World.AccessBindings.Remove(InChange.InventoryId);
        World.Tombstones.Add(InChange.InventoryId, FInventoryTombstone{InChange.Generation, InChange.AfterRevision});
        return true;
    }
    FInventorySnapshot After;
    if(!FInventorySnapshotCodec::Decode(InChange.AfterBytes, World.Registry, World.Profiles, After, OutError) ||
       After.WorldId != World.WorldId || After.InventoryId != InChange.InventoryId || After.Revision != InChange.AfterRevision ||
       After.Generation != InChange.Generation || World.Tombstones.Contains(InChange.InventoryId))
    {
        OutError = TEXT("Inventory.InventoryRedoMalformed");
        return false;
    }
    FInventorySnapshot Before;
    FString LookupError;
    const bool bExists = World.GetSnapshot(InChange.InventoryId, Before, LookupError);
    if(bExists && Before.Revision == InChange.AfterRevision)
    {
        if(Before == After)
        {
            return true;
        }
        OutError = TEXT("Inventory.InventoryRedoDiverged");
        return false;
    }
    if((InChange.bCreated && bExists) || (!InChange.bCreated && (!bExists || Before.Revision != InChange.BeforeRevision || Before.Generation != InChange.Generation)))
    {
        OutError = TEXT("Inventory.InventoryRedoRevisionStale");
        return false;
    }
    if(TObjectPtr<UAbilityInventoryBase>* Inventory = World.Active.Find(InChange.InventoryId))
    {
        (*Inventory)->State = MoveTemp(After);
    }
    else
    {
        World.Cold.Add(InChange.InventoryId, InChange.AfterBytes);
    }
    return true;
}

bool FInventoryCommitCoordinator::Replay(const FInventoryJournalRecord& InRecord, FString& OutError)
{
    if(InRecord.WorldId != World.WorldId)
    {
        OutError = TEXT("Inventory.ReplayWorldMismatch");
        return false;
    }
    for(const FInventoryJournalChange& Change : InRecord.Inventories)
    {
        if(!ApplyInventory(Change, OutError))
        {
            return false;
        }
    }
    for(const FInventoryDomainRedo& Redo : InRecord.Domains)
    {
        if(!World.Records.Apply(Redo, OutError))
        {
            return false;
        }
    }
    return true;
}

FInventoryCommandResult FInventoryCommitCoordinator::Commit(FInventoryPlan InPlan, const FInventoryBatchRequest& InRequest, FString& OutError)
{
    FInventoryCommandResult Result;
    Result.TransactionId = FGuid::NewGuid();
    Result.RequestId = InRequest.RequestId;
    Result.Code = EInventoryResultCode::CommitFailed;
    FInventoryJournalRecord Record;
    Record.WorldId = World.WorldId;
    Record.TransactionId = Result.TransactionId;
    Record.Domains = InRequest.DomainChanges;
    InPlan.Changes.TransactionId = Record.TransactionId;
    TSet<FGuid> Created;
    for(const FInventorySnapshot& Snapshot : InRequest.CreateInventories)
    {
        Created.Add(Snapshot.InventoryId);
    }
    TSet<FGuid> Deleted;
    for(const FGuid& InventoryId : InRequest.DeleteInventories)
    {
        Deleted.Add(InventoryId);
    }
    InPlan.After.Sort([](const FInventorySnapshot& InLeft, const FInventorySnapshot& InRight)
    {
        return InventoryOrder::GuidLess(InLeft.InventoryId, InRight.InventoryId);
    });
    for(const FInventorySnapshot& After : InPlan.After)
    {
        const FInventorySnapshot* Before = InPlan.Before.FindByPredicate([&After](const FInventorySnapshot& Candidate)
        {
            return Candidate.InventoryId == After.InventoryId;
        });
        if(!Before || Before->Revision == MAX_uint64 || After.Revision != Before->Revision + 1)
        {
            OutError = TEXT("Inventory.PlanRevisionInvalid");
            return Result;
        }
        FInventoryJournalChange Change;
        Change.InventoryId = After.InventoryId;
        Change.BeforeRevision = Before->Revision;
        Change.AfterRevision = After.Revision;
        Change.Generation = After.Generation;
        Change.bCreated = Created.Contains(After.InventoryId);
        Change.bDeleted = Deleted.Contains(After.InventoryId);
        if(!Change.bDeleted && !FInventorySnapshotCodec::Encode(After, World.Registry, World.Profiles, Change.AfterBytes, OutError))
        {
            return Result;
        }
        Record.Inventories.Add(MoveTemp(Change));
    }
    TMap<FName, TArray<FInventoryDomainRedo>> AdapterChanges;
    for(const FInventoryDomainRedo& Redo : Record.Domains)
    {
        if(World.Adapters.Contains(Redo.Domain))
        {
            AdapterChanges.FindOrAdd(Redo.Domain).Add(Redo);
        }
    }
    TArray<FName> AdapterNames;
    AdapterChanges.GetKeys(AdapterNames);
    AdapterNames.Sort(FNameLexicalLess());
    TArray<FName> PreparedNames;
    bool bDurable = false;
    ON_SCOPE_EXIT
    {
        if(!bDurable)
        {
            for(FName Name : PreparedNames)
            {
                World.Adapters.FindChecked(Name)->Cancel(Record.TransactionId);
            }
        }
    };
    for(FName Name : AdapterNames)
    {
        PreparedNames.Add(Name);
        if(!World.Adapters.FindChecked(Name)->Prepare(Record.TransactionId, AdapterChanges.FindChecked(Name), OutError))
        {
            Result.Code = EInventoryResultCode::SourcePreparationFailed;
            return Result;
        }
    }
    for(FName Name : PreparedNames)
    {
        if(!World.Adapters.FindChecked(Name)->ValidatePrepared(Record.TransactionId, OutError))
        {
            Result.Code = EInventoryResultCode::RevisionStale;
            return Result;
        }
    }
    for(const FInventorySnapshot& Before : InPlan.Before)
    {
        if(Created.Contains(Before.InventoryId))
        {
            if(World.Active.Contains(Before.InventoryId) || World.Cold.Contains(Before.InventoryId) || World.Tombstones.Contains(Before.InventoryId))
            {
                Result.Code = EInventoryResultCode::GenerationChanged;
                return Result;
            }
            continue;
        }
        FInventorySnapshot Current;
        if(!World.GetSnapshot(Before.InventoryId, Current, OutError) || Current != Before)
        {
            Result.Code = EInventoryResultCode::RevisionStale;
            return Result;
        }
    }
    for(const FInventoryExpectedRevision& Expected : InRequest.ExpectedRevisions)
    {
        if(Created.Contains(Expected.InventoryId)) continue;
        FInventorySnapshot Current;
        if(!World.GetSnapshot(Expected.InventoryId, Current, OutError) || Current.Generation != Expected.Generation || Current.Revision != Expected.Revision)
        {
            Result.Code = EInventoryResultCode::RevisionStale;
            return Result;
        }
    }
    for(const FInventoryDomainRedo& Redo : Record.Domains)
    {
        if(!World.Records.ValidateRedo(Redo, OutError))
        {
            Result.Code = EInventoryResultCode::RevisionStale;
            return Result;
        }
    }
    if(!World.IsReady() || !World.ValidateInstances(InPlan.Before, InPlan.After, OutError))
    {
        Result.Code = EInventoryResultCode::InvalidState;
        return Result;
    }
    if(!World.Journal.Append(Record, OutError))
    {
        Result.Code = EInventoryResultCode::SaveUnavailable;
        if(!World.Journal.IsHealthy())
        {
            Result.Code = EInventoryResultCode::ConfirmationPending;
            World.Quarantine(OutError);
        }
        return Result;
    }
    bDurable = true;
    Result.Code = EInventoryResultCode::ConfirmationPending;
    if(!Replay(Record, OutError))
    {
        World.Quarantine(OutError);
        return Result;
    }
    for(const FInventoryDomainRedo& Redo : Record.Domains)
    {
        const TSharedRef<IInventoryDomainAdapter>* Adapter = World.Adapters.Find(Redo.Domain);
        if(Adapter && !(*Adapter)->ApplyCommitted(Record.TransactionId, Redo, OutError))
        {
            World.Quarantine(OutError);
            return Result;
        }
    }
    for(const FInventorySnapshot& Before : InPlan.Before)
    {
        for(const FInventorySlotState& Slot : Before.Slots)
        {
            if(Slot.Item.InstanceId.IsValid())
            {
                World.InstanceOwners.Remove(Slot.Item.InstanceId);
            }
        }
    }
    for(const FInventorySnapshot& After : InPlan.After)
    {
        if(Deleted.Contains(After.InventoryId))
        {
            continue;
        }
        for(const FInventorySlotState& Slot : After.Slots)
        {
            if(Slot.Item.InstanceId.IsValid())
            {
                World.InstanceOwners.Add(Slot.Item.InstanceId, After.InventoryId);
            }
        }
    }
    Result.Code = InPlan.Code;
    Result.AcceptedCount = InPlan.AcceptedCount;
    Result.RemainingCount = InPlan.RemainingCount;
    Result.ChangedSlots = InPlan.Changes.ChangedSlots;
    Result.FinalRevisions = InPlan.Changes.Revisions;
    TArray<FInventoryDomainRecord> ChangedRecords;
    for(const FInventoryDomainRedo& Redo : Record.Domains)
    {
        ChangedRecords.Add(*World.Records.Find(Redo.Domain, Redo.EntityId));
    }
    World.bCommitting = false;
    TGuardValue<bool> PublishingGuard(World.bPublishing, true);
    World.OnCommitted.Broadcast(InPlan.Changes);
    if(!ChangedRecords.IsEmpty())
    {
        World.OnRecordsCommitted.Broadcast(ChangedRecords);
    }
    for(FName Name : PreparedNames)
    {
        World.Adapters.FindChecked(Name)->Publish(Record.TransactionId);
    }
    for(const FGuid& InventoryId : InRequest.DeleteInventories)
    {
        World.OnDeleted.Broadcast(InventoryId);
    }
    return Result;
}
