#pragma once

#include "Ability/Inventory/InventoryCommandTypes.h"

struct WHFRAMEWORK_API FInventoryOperationResult
{
    EInventoryResultCode Code = EInventoryResultCode::None;
    int64 AcceptedCount = 0;
    int64 RemainingCount = 0;
};

struct WHFRAMEWORK_API FInventoryQuantityDelta
{
    FInventoryStackSignature Signature;
    int64 Minted = 0;
    int64 Consumed = 0;
};

struct WHFRAMEWORK_API FInventoryPlan
{
    EInventoryResultCode Code = EInventoryResultCode::None;
    int64 AcceptedCount = 0;
    int64 RemainingCount = 0;
    TArray<FInventorySnapshot> Before;
    TArray<FInventorySnapshot> After;
    FInventoryChangeSet Changes;
    TArray<FInventoryOperationResult> Operations;
    TArray<FInventoryQuantityDelta> QuantityDeltas;
};
