#pragma once

#include "Ability/Inventory/InventoryPlanner.h"

class FInventoryPlanBuilder
{
public:
    explicit FInventoryPlanBuilder(const FInventoryPlannerContext& InContext);
    EInventoryResultCode Build(TConstArrayView<FInventorySnapshot> InStates, TConstArrayView<FInventoryOperation> InOperations, FInventoryPlan& OutPlan, FString& OutError);

private:
    EInventoryResultCode Initialize(TConstArrayView<FInventorySnapshot> InStates, FString& OutError);
    EInventoryResultCode Apply(const FInventoryOperation& InOperation, FInventoryOperationResult& OutResult);
    EInventoryResultCode Add(int32 InTargetIndex, const FAbilityItem& InItem, int32 InRequested, EInventoryAmountMode InMode, const FInventoryOperation& InOperation, bool bInFavorite, int32& OutAccepted);
    EInventoryResultCode Remove(int32 InSourceIndex, const FInventoryOperation& InOperation, FInventoryOperationResult& OutResult);
    EInventoryResultCode Move(const FInventoryOperation& InOperation, FInventoryOperationResult& OutResult);
    EInventoryResultCode Transfer(const FInventoryOperation& InOperation, FInventoryOperationResult& OutResult);
    EInventoryResultCode Exchange(const FInventoryOperation& InOperation, FInventoryOperationResult& OutResult);
    EInventoryResultCode Swap(const FInventoryOperation& InOperation, FInventoryOperationResult& OutResult);
    EInventoryResultCode Sort(const FInventoryOperation& InOperation, FInventoryOperationResult& OutResult);
    EInventoryResultCode QuickStack(const FInventoryOperation& InOperation, FInventoryOperationResult& OutResult);
    EInventoryResultCode SetFavorite(const FInventoryOperation& InOperation, FInventoryOperationResult& OutResult);
    EInventoryResultCode Transform(const FInventoryOperation& InOperation, FInventoryOperationResult& OutResult);
    EInventoryResultCode GetCandidates(int32 InStateIndex, const FInventoryOperation& InOperation, bool bInTarget, TArray<int32>& OutIndices) const;
    EInventoryResultCode CanTake(int32 InStateIndex, int32 InSlotIndex, const FInventoryOperation& InOperation) const;
    EInventoryResultCode CanPut(int32 InStateIndex, int32 InSlotIndex, const FAbilityItem& InItem, const FInventoryOperation& InOperation) const;
    int32 FindState(const FGuid& InInventoryId) const;
    int32 FindSlot(int32 InStateIndex, const FInventorySlotKey& InKey) const;
    const FInventoryProfileDefinition& GetProfile(int32 InStateIndex) const;
    const FInventoryRuleProvider& GetRules() const;
    void ReduceSlot(int32 InStateIndex, int32 InSlotIndex, int32 InCount);
    void TrackQuantity(const FAbilityItem& InItem, int64 InMinted, int64 InConsumed);
    EInventoryResultCode Finalize(FInventoryPlan& OutPlan, FString& OutError);

    const FInventoryPlannerContext& Context;
    FInventoryRuleProvider DefaultRules;
    TArray<FInventorySnapshot> Original;
    TArray<FInventorySnapshot> Working;
    TMap<FGuid, int32> StateIndices;
    FInventoryPlan Plan;
};
