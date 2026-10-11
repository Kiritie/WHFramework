#include "Ability/Inventory/InventoryPlanner.h"

#include "InventoryPlanBuilder.h"

FInventoryRuleProvider::~FInventoryRuleProvider()
{
}

EInventoryResultCode FInventoryRuleProvider::CanTake(const FInventorySnapshot& InState, const FInventorySlotState& InSlot, const FInventoryOperation& InOperation, const FInventoryItemRegistry& InRegistry) const
{
    const FInventoryItemDefinition* Definition = InRegistry.Find(InSlot.Item.ItemId);
    if (!Definition || InSlot.Item.IsEmpty())
    {
        return EInventoryResultCode::InvalidItem;
    }
    if (InOperation.Type == EInventoryOperationType::Exchange && !Definition->bCanTrade)
    {
        return EInventoryResultCode::Unauthorized;
    }
    const FGuid BoundOwner = InSlot.Item.GetBoundOwner();
    if (BoundOwner.IsValid() && BoundOwner != InState.OwnerId)
    {
        return EInventoryResultCode::ItemBound;
    }
    return EInventoryResultCode::Success;
}

EInventoryResultCode FInventoryRuleProvider::CanPut(const FInventorySnapshot& InState, const FInventorySlotState& InSlot, const FAbilityItem& InItem, const FInventoryOperation& InOperation, const FInventoryItemRegistry& InRegistry) const
{
    if (!InRegistry.Find(InItem.ItemId))
    {
        return EInventoryResultCode::InvalidItem;
    }
    const FGuid BoundOwner = InItem.GetBoundOwner();
    if (BoundOwner.IsValid() && BoundOwner != InState.OwnerId)
    {
        return EInventoryResultCode::ItemBound;
    }
    return EInventoryResultCode::Success;
}

EInventoryResultCode FInventoryRuleProvider::ValidateFinalState(const FInventorySnapshot& InState, const FInventoryProfileDefinition& InProfile, const FInventoryItemRegistry& InRegistry) const
{
    for (int32 SlotIndex = 0; SlotIndex < InState.Slots.Num(); ++SlotIndex)
    {
        const FInventorySlotState& Slot = InState.Slots[SlotIndex];
        if (Slot.Item.IsEmpty())
        {
            continue;
        }
        const FGuid BoundOwner = Slot.Item.GetBoundOwner();
        if (BoundOwner.IsValid() && BoundOwner != InState.OwnerId)
        {
            return EInventoryResultCode::ItemBound;
        }
        const FInventoryItemDefinition* Definition = InRegistry.Find(Slot.Item.ItemId);
        if (!Definition)
        {
            return EInventoryResultCode::InvalidItem;
        }
        const FInventorySlotRule& Rule = InProfile.Slots[SlotIndex].Rule;
        if (Rule.OppositeHandSlot.IsValid() && Definition->HandPolicy == EInventoryHandPolicy::TwoHand)
        {
            const FInventorySlotState* Opposite = InState.FindSlot(Rule.OppositeHandSlot);
            if (!Opposite || !Opposite->Item.IsEmpty())
            {
                return EInventoryResultCode::SlotRuleRejected;
            }
        }
    }
    return EInventoryResultCode::Success;
}

FInventoryPlannerContext::FInventoryPlannerContext(const FInventoryItemRegistry& InRegistry, const TMap<FPrimaryAssetId, FInventoryProfileDefinition>& InProfiles, const FInventoryRuleProvider* InRules)
    : Registry(InRegistry)
    , Profiles(InProfiles)
    , Rules(InRules)
{
}

EInventoryResultCode FInventoryPlanner::PlanBatch(const FInventoryPlannerContext& InContext, TConstArrayView<FInventorySnapshot> InStates, TConstArrayView<FInventoryOperation> InOperations, FInventoryPlan& OutPlan, FString& OutError)
{
    FInventoryPlanBuilder Builder(InContext);
    return Builder.Build(InStates, InOperations, OutPlan, OutError);
}

EInventoryResultCode FInventoryPlanner::Query(const FInventoryPlannerContext& InContext, const FInventorySnapshot& InState, const FInventoryQueryRequest& InRequest, FInventoryQueryResult& OutResult, FString& OutError)
{
    OutResult = FInventoryQueryResult();
    OutError.Reset();
    const FInventoryProfileDefinition* Profile = InContext.Profiles.Find(InState.ProfileId);
    if (!Profile || !Profile->ValidateState(InState, InContext.Registry, OutError))
    {
        return EInventoryResultCode::InvalidState;
    }
    const bool bSpecifiedSlot = InRequest.SpecificSlot.Group.IsValid() || InRequest.SpecificSlot.Index != INDEX_NONE;
    if ((bSpecifiedSlot && (!InRequest.SpecificSlot.IsValid() || !Profile->FindSlot(InRequest.SpecificSlot)))
        || InRequest.AllowedGroups.Num() > 64 || (InRequest.Filter.bMatchLevel && InRequest.Filter.Level < 0))
    {
        return EInventoryResultCode::InvalidRequest;
    }
    TSet<FGameplayTag> SeenGroups;
    for (const FGameplayTag Group : InRequest.AllowedGroups)
    {
        if (!Group.IsValid() || SeenGroups.Contains(Group)
            || !Profile->Slots.ContainsByPredicate([Group](const FInventorySlotDefinition& InSlot)
            {
                return InSlot.Key.Group == Group;
            }))
        {
            return EInventoryResultCode::InvalidRequest;
        }
        SeenGroups.Add(Group);
    }
    const bool bQueryCapacity = !InRequest.CapacityItem.IsEmpty();
    if (bQueryCapacity && !InContext.Registry.ValidateItem(InRequest.CapacityItem, OutError))
    {
        return EInventoryResultCode::InvalidItem;
    }
    FInventoryRuleProvider DefaultRules;
    const FInventoryRuleProvider& Rules = InContext.Rules ? *InContext.Rules : DefaultRules;
    FInventoryOperation QueryOperation;
    QueryOperation.Type = EInventoryOperationType::Add;
    QueryOperation.bAutomatic = InRequest.bAutomatic;
    const FInventoryItemDefinition* CapacityDefinition = bQueryCapacity ? InContext.Registry.Find(InRequest.CapacityItem.ItemId) : nullptr;
    bool bUniqueAlreadyPresent = false;
    if (bQueryCapacity && InRequest.CapacityItem.InstanceId.IsValid())
    {
        bUniqueAlreadyPresent = InState.Slots.ContainsByPredicate([&InRequest](const FInventorySlotState& InSlot)
        {
            return InSlot.Item.InstanceId == InRequest.CapacityItem.InstanceId;
        });
    }
    for (int32 SlotIndex = 0; SlotIndex < InState.Slots.Num(); ++SlotIndex)
    {
        const FInventorySlotState& Slot = InState.Slots[SlotIndex];
        const FInventorySlotRule& Rule = Profile->Slots[SlotIndex].Rule;
        if ((bSpecifiedSlot && Slot.Key != InRequest.SpecificSlot)
            || (!InRequest.AllowedGroups.IsEmpty() && !InRequest.AllowedGroups.Contains(Slot.Key.Group)))
        {
            continue;
        }
        const bool bAutomaticProtected = InRequest.bAutomatic && (Slot.bFavorite || Slot.bLocked || !Rule.bAllowsAutomatic);
        if (!bAutomaticProtected && InRequest.Filter.Matches(Slot.Item))
        {
            OutResult.MatchingCount += Slot.Item.Count;
            OutResult.Matches.Add(Slot);
        }
        if (!CapacityDefinition || bUniqueAlreadyPresent || Slot.bLocked || bAutomaticProtected
            || !Rule.bAllowsInsert || !Rule.Accepts(*CapacityDefinition)
            || Rules.CanPut(InState, Slot, InRequest.CapacityItem, QueryOperation, InContext.Registry) != EInventoryResultCode::Success)
        {
            continue;
        }
        if (Slot.Item.IsEmpty())
        {
            OutResult.Capacity += Rule.GetCapacity(*CapacityDefinition);
        }
        else if (Slot.Item.CanStackWith(InRequest.CapacityItem))
        {
            OutResult.Capacity += Rule.GetCapacity(*CapacityDefinition) - Slot.Item.Count;
        }
    }
    if (bQueryCapacity && InRequest.CapacityItem.InstanceId.IsValid())
    {
        OutResult.Capacity = FMath::Min<int64>(OutResult.Capacity, 1);
    }
    OutResult.ObservedRevision = InState.Revision;
    return EInventoryResultCode::Success;
}

bool FInventoryPlanner::ValidateUniqueInstances(TConstArrayView<FInventorySnapshot> InStates, FString& OutError)
{
    OutError.Reset();
    TSet<FGuid> Instances;
    for (const FInventorySnapshot& State : InStates)
    {
        for (const FInventorySlotState& Slot : State.Slots)
        {
            if (!Slot.Item.InstanceId.IsValid())
            {
                continue;
            }
            if (Instances.Contains(Slot.Item.InstanceId))
            {
                OutError = TEXT("同一唯一物品实例同时出现在多个格位或库存中。");
                return false;
            }
            Instances.Add(Slot.Item.InstanceId);
        }
    }
    return true;
}

bool FInventoryPlanner::ValidateConservation(TConstArrayView<FInventorySnapshot> InBefore, TConstArrayView<FInventorySnapshot> InAfter, TConstArrayView<FInventoryQuantityDelta> InDeltas, FString& OutError)
{
    OutError.Reset();
    TMap<FInventoryStackSignature, int64> Balances;
    auto AddBalance = [&Balances, &OutError](const FInventoryStackSignature& InSignature, const int64 InDelta)
    {
        int64& Balance = Balances.FindOrAdd(InSignature);
        if ((InDelta > 0 && Balance > MAX_int64 - InDelta)
            || (InDelta < 0 && Balance < MIN_int64 - InDelta))
        {
            OutError = TEXT("物品守恒统计超出 int64 上限。");
            return false;
        }
        Balance += InDelta;
        return true;
    };
    for (const FInventorySnapshot& State : InBefore)
    {
        for (const FInventorySlotState& Slot : State.Slots)
        {
            if (!Slot.Item.IsEmpty() && !AddBalance(Slot.Item.Signature(), Slot.Item.Count))
            {
                return false;
            }
        }
    }
    for (const FInventoryQuantityDelta& Delta : InDeltas)
    {
        if (Delta.Minted < 0 || Delta.Consumed < 0 || !Delta.Signature.ItemId.IsValid()
            || !AddBalance(Delta.Signature, Delta.Minted) || !AddBalance(Delta.Signature, -Delta.Consumed))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("物品授权生成或消费计量无效。");
            }
            return false;
        }
    }
    for (const FInventorySnapshot& State : InAfter)
    {
        for (const FInventorySlotState& Slot : State.Slots)
        {
            if (!Slot.Item.IsEmpty() && !AddBalance(Slot.Item.Signature(), -static_cast<int64>(Slot.Item.Count)))
            {
                return false;
            }
        }
    }
    for (const TPair<FInventoryStackSignature, int64>& Pair : Balances)
    {
        if (Pair.Value != 0)
        {
            OutError = TEXT("事务不符合按物品签名统计的数量守恒。");
            return false;
        }
    }
    return true;
}
