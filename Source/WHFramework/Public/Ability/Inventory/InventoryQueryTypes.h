#pragma once

#include "Ability/Inventory/InventorySnapshotTypes.h"

struct WHFRAMEWORK_API FInventoryQueryRequest
{
    FInventorySlotKey SpecificSlot;
    TArray<FGameplayTag> AllowedGroups;
    FInventoryItemFilter Filter;
    FAbilityItem CapacityItem;
    bool bAutomatic = false;
};

struct WHFRAMEWORK_API FInventoryQueryResult
{
    int64 MatchingCount = 0;
    int64 Capacity = 0;
    uint64 ObservedRevision = 0;
    TArray<FInventorySlotState> Matches;
};
