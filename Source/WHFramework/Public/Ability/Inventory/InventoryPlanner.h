#pragma once

#include "CoreMinimal.h"
#include "Ability/Inventory/InventoryItemRegistry.h"
#include "Ability/Inventory/InventoryProfile.h"

class WHFRAMEWORK_API FInventoryRuleProvider
{
public:
    virtual ~FInventoryRuleProvider();
    virtual EInventoryResultCode CanTake(const FInventorySnapshot& InState, const FInventorySlotState& InSlot, const FInventoryOperation& InOperation, const FInventoryItemRegistry& InRegistry) const;
    virtual EInventoryResultCode CanPut(const FInventorySnapshot& InState, const FInventorySlotState& InSlot, const FAbilityItem& InItem, const FInventoryOperation& InOperation, const FInventoryItemRegistry& InRegistry) const;
    virtual EInventoryResultCode ValidateFinalState(const FInventorySnapshot& InState, const FInventoryProfileDefinition& InProfile, const FInventoryItemRegistry& InRegistry) const;
};

struct WHFRAMEWORK_API FInventoryPlannerContext
{
    FInventoryPlannerContext(const FInventoryItemRegistry& InRegistry, const TMap<FPrimaryAssetId, FInventoryProfileDefinition>& InProfiles, const FInventoryRuleProvider* InRules = nullptr);

    const FInventoryItemRegistry& Registry;
    const TMap<FPrimaryAssetId, FInventoryProfileDefinition>& Profiles;
    const FInventoryRuleProvider* Rules;
};

class WHFRAMEWORK_API FInventoryPlanner
{
public:
    static EInventoryResultCode PlanBatch(const FInventoryPlannerContext& InContext, TConstArrayView<FInventorySnapshot> InStates, TConstArrayView<FInventoryOperation> InOperations, FInventoryPlan& OutPlan, FString& OutError);
    static EInventoryResultCode Query(const FInventoryPlannerContext& InContext, const FInventorySnapshot& InState, const FInventoryQueryRequest& InRequest, FInventoryQueryResult& OutResult, FString& OutError);
    static bool ValidateUniqueInstances(TConstArrayView<FInventorySnapshot> InStates, FString& OutError);
    static bool ValidateConservation(TConstArrayView<FInventorySnapshot> InBefore, TConstArrayView<FInventorySnapshot> InAfter, TConstArrayView<FInventoryQuantityDelta> InDeltas, FString& OutError);
};
