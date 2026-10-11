#include "Ability/Inventory/InventoryProfile.h"

#include "Ability/Inventory/InventoryItemRegistry.h"

int32 FInventorySlotRule::GetCapacity(const FInventoryItemDefinition& InDefinition) const
{
    return MaxStackOverride > 0 ? FMath::Min(MaxStackOverride, InDefinition.MaxStack) : InDefinition.MaxStack;
}

bool FInventorySlotRule::Accepts(const FInventoryItemDefinition& InDefinition) const
{
    return InDefinition.Tags.HasAll(RequiredAllTags)
        && (RequiredAnyTags.IsEmpty() || InDefinition.Tags.HasAny(RequiredAnyTags))
        && !InDefinition.Tags.HasAny(BlockedTags)
        && (!EquipPart.IsValid() || InDefinition.EquipPart == EquipPart);
}

bool FInventorySlotRule::operator==(const FInventorySlotRule& InOther) const
{
    return RequiredAllTags == InOther.RequiredAllTags && RequiredAnyTags == InOther.RequiredAnyTags
        && BlockedTags == InOther.BlockedTags && MaxStackOverride == InOther.MaxStackOverride
        && bAllowsInsert == InOther.bAllowsInsert && bAllowsRemove == InOther.bAllowsRemove
        && bAllowsAutomatic == InOther.bAllowsAutomatic && bAllowsSort == InOther.bAllowsSort
        && bInitiallyLocked == InOther.bInitiallyLocked && EquipPart == InOther.EquipPart
        && WeaponGroup == InOther.WeaponGroup && OppositeHandSlot == InOther.OppositeHandSlot;
}

bool FInventoryProfileDefinition::Build(const FPrimaryAssetId& InProfileId, TConstArrayView<FInventorySlotGroupDefinition> InGroups, FInventoryProfileDefinition& OutDefinition, FString& OutError)
{
    OutError.Reset();
    OutDefinition = FInventoryProfileDefinition();
    if (!InProfileId.IsValid() || InGroups.IsEmpty() || InGroups.Num() > 64)
    {
        OutError = TEXT("库存布局身份或分组数量无效。");
        return false;
    }
    FInventoryProfileDefinition Prepared;
    Prepared.ProfileId = InProfileId;
    TSet<FGameplayTag> SeenGroups;
    for (const FInventorySlotGroupDefinition& Group : InGroups)
    {
        if (!Group.Group.IsValid() || SeenGroups.Contains(Group.Group)
            || Group.Count < 1 || Group.Count > 4096 || Prepared.Slots.Num() + Group.Count > 4096)
        {
            OutError = TEXT("库存分组重复、容量无效或超出格位上限。");
            return false;
        }
        TSet<int32> OverrideIndices;
        for (const FInventorySlotRuleOverride& Override : Group.Overrides)
        {
            if (Override.Index < 0 || Override.Index >= Group.Count || OverrideIndices.Contains(Override.Index))
            {
                OutError = TEXT("格位规则覆盖包含重复或越界索引。");
                return false;
            }
            OverrideIndices.Add(Override.Index);
        }
        for (int32 SlotIndex = 0; SlotIndex < Group.Count; ++SlotIndex)
        {
            FInventorySlotDefinition Slot;
            Slot.Key.Group = Group.Group;
            Slot.Key.Index = SlotIndex;
            Slot.Order = Group.Order;
            Slot.AddPriority = Group.AddPriority;
            Slot.RemovePriority = Group.RemovePriority;
            Slot.Rule = Group.DefaultRule;
            const FInventorySlotRuleOverride* Override = Group.Overrides.FindByPredicate([SlotIndex](const FInventorySlotRuleOverride& InOverride)
            {
                return InOverride.Index == SlotIndex;
            });
            if (Override)
            {
                Slot.Rule = Override->Rule;
            }
            Prepared.Slots.Add(MoveTemp(Slot));
        }
        SeenGroups.Add(Group.Group);
    }
    Prepared.Slots.Sort([](const FInventorySlotDefinition& InLeft, const FInventorySlotDefinition& InRight)
    {
        if (InLeft.Order != InRight.Order)
        {
            return InLeft.Order < InRight.Order;
        }
        return InventoryOrder::SlotKeyLess(InLeft.Key, InRight.Key);
    });
    if (!Prepared.ValidateDefinition(OutError))
    {
        return false;
    }
    OutDefinition = MoveTemp(Prepared);
    return true;
}

bool FInventoryProfileDefinition::ValidateDefinition(FString& OutError) const
{
    OutError.Reset();
    if (!ProfileId.IsValid() || ProfileId.ToString().Len() > 256 || Slots.IsEmpty() || Slots.Num() > 4096)
    {
        OutError = TEXT("已编译库存布局无效。");
        return false;
    }
    TMap<FGameplayTag, int32> NextIndices;
    TMap<FGameplayTag, int32> GroupOrders;
    TSet<FInventorySlotKey> SeenSlots;
    for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
    {
        const FInventorySlotDefinition& Slot = Slots[SlotIndex];
        if (!Slot.Key.IsValid() || Slot.Key.Group.ToString().Len() > 256 || SeenSlots.Contains(Slot.Key)
            || Slot.Rule.MaxStackOverride < 0 || Slot.Rule.WeaponGroup < INDEX_NONE)
        {
            OutError = TEXT("库存格位身份、容量覆盖或武器组无效。");
            return false;
        }
        int32& NextIndex = NextIndices.FindOrAdd(Slot.Key.Group);
        if (Slot.Key.Index != NextIndex)
        {
            OutError = TEXT("每个库存组的索引必须从零连续递增。");
            return false;
        }
        ++NextIndex;
        const int32* ExistingOrder = GroupOrders.Find(Slot.Key.Group);
        if (ExistingOrder && *ExistingOrder != Slot.Order)
        {
            OutError = TEXT("同一库存组的顺序不一致。");
            return false;
        }
        GroupOrders.Add(Slot.Key.Group, Slot.Order);
        if (SlotIndex > 0)
        {
            const FInventorySlotDefinition& Previous = Slots[SlotIndex - 1];
            if (Previous.Order > Slot.Order
                || (Previous.Order == Slot.Order && !InventoryOrder::SlotKeyLess(Previous.Key, Slot.Key)))
            {
                OutError = TEXT("库存格位不符合布局的稳定顺序。");
                return false;
            }
        }
        const bool bNoOpposite = !Slot.Rule.OppositeHandSlot.Group.IsValid()
            && Slot.Rule.OppositeHandSlot.Index == INDEX_NONE;
        if (!bNoOpposite)
        {
            const FInventorySlotDefinition* Opposite = FindSlot(Slot.Rule.OppositeHandSlot);
            if (!Slot.Rule.OppositeHandSlot.IsValid() || !Opposite || Opposite->Key == Slot.Key
                || Opposite->Rule.OppositeHandSlot != Slot.Key
                || Opposite->Rule.WeaponGroup != Slot.Rule.WeaponGroup)
            {
                OutError = TEXT("双手装备槽必须配置同组互相对应的另一手格位。");
                return false;
            }
        }
        SeenSlots.Add(Slot.Key);
    }
    return true;
}

bool FInventoryProfileDefinition::ValidateState(const FInventorySnapshot& InState, const FInventoryItemRegistry& InRegistry, FString& OutError) const
{
    OutError.Reset();
    if (InState.SchemaVersion != 2 || !InState.InventoryId.IsValid() || !InState.WorldId.IsValid()
        || InState.ProfileId != ProfileId || InState.Generation == 0 || InState.Slots.Num() != Slots.Num())
    {
        OutError = TEXT("库存快照的版本、身份、代次或格位布局无效。");
        return false;
    }
    TSet<FGuid> Instances;
    for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
    {
        const FInventorySlotState& Slot = InState.Slots[SlotIndex];
        if (Slot.Key != Slots[SlotIndex].Key || !InRegistry.ValidateItem(Slot.Item, OutError))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("库存格位与布局定义不匹配。");
            }
            return false;
        }
        if (Slot.Item.IsEmpty())
        {
            if (Slot.bFavorite)
            {
                OutError = TEXT("空格位不能持有物品收藏标记。");
                return false;
            }
            continue;
        }
        const FInventoryItemDefinition* Definition = InRegistry.Find(Slot.Item.ItemId);
        if (!Definition || !Slots[SlotIndex].Rule.Accepts(*Definition)
            || Slot.Item.Count > Slots[SlotIndex].Rule.GetCapacity(*Definition))
        {
            OutError = TEXT("库存物品不符合格位接收规则或堆叠上限。");
            return false;
        }
        if (Slot.Item.InstanceId.IsValid())
        {
            if (Instances.Contains(Slot.Item.InstanceId))
            {
                OutError = TEXT("同一库存内出现重复物品实例。");
                return false;
            }
            Instances.Add(Slot.Item.InstanceId);
        }
    }
    return true;
}

const FInventorySlotDefinition* FInventoryProfileDefinition::FindSlot(const FInventorySlotKey& InKey) const
{
    return Slots.FindByPredicate([&InKey](const FInventorySlotDefinition& InSlot)
    {
        return InSlot.Key == InKey;
    });
}

FInventorySnapshot FInventoryProfileDefinition::CreateState(const FGuid& InInventoryId, const FGuid& InWorldId, const FGuid& InOwnerId, const uint32 InGeneration) const
{
    FInventorySnapshot State;
    State.InventoryId = InInventoryId;
    State.WorldId = InWorldId;
    State.OwnerId = InOwnerId;
    State.ProfileId = ProfileId;
    State.Generation = InGeneration;
    State.Slots.Reserve(Slots.Num());
    for (const FInventorySlotDefinition& Definition : Slots)
    {
        FInventorySlotState Slot;
        Slot.Key = Definition.Key;
        Slot.bLocked = Definition.Rule.bInitiallyLocked;
        State.Slots.Add(MoveTemp(Slot));
    }
    return State;
}

FPrimaryAssetId UInventoryProfile::GetPrimaryAssetId() const
{
    return FPrimaryAssetId(FPrimaryAssetType(TEXT("InventoryProfile")), GetFName());
}

bool UInventoryProfile::Compile(FInventoryProfileDefinition& OutDefinition, FString& OutError) const
{
    return FInventoryProfileDefinition::Build(GetPrimaryAssetId(), Groups, OutDefinition, OutError);
}
