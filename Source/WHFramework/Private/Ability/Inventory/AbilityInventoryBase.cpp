#include "Ability/Inventory/AbilityInventoryBase.h"

#include "Ability/Inventory/InventoryWorldSubsystem.h"

FGuid UAbilityInventoryBase::GetInventoryId() const
{
    return State.InventoryId;
}

uint64 UAbilityInventoryBase::GetRevision() const
{
    return State.Revision;
}

FInventorySnapshot UAbilityInventoryBase::MakeSnapshot() const
{
    return State;
}

const FInventorySlotState* UAbilityInventoryBase::FindSlot(const FInventorySlotKey& InKey) const
{
    return State.FindSlot(InKey);
}

FInventoryQueryResult UAbilityInventoryBase::Query(const FInventoryQueryRequest& InRequest) const
{
    FInventoryQueryResult Result;
    const UInventoryWorldSubsystem* Owner = OwnerSubsystem.Get();
    if(!Owner || !Owner->IsReady())
    {
        return Result;
    }
    FString Error;
    const FInventoryPlannerContext Context(Owner->GetRegistry(), Owner->GetProfiles(), Owner->GetRules());
    FInventoryPlanner::Query(Context, State, InRequest, Result, Error);
    return Result;
}
