#include "Ability/Inventory/InventoryItemRegistry.h"

bool FInventoryItemRegistry::Initialize(TConstArrayView<FInventoryItemDefinition> InDefinitions, FString& OutError)
{
    OutError.Reset();
    if (bInitialized)
    {
        OutError = TEXT("物品注册表已冻结，不能在运行期间替换规则。");
        return false;
    }
    if (InDefinitions.IsEmpty() || InDefinitions.Num() > 65536)
    {
        OutError = TEXT("物品定义数量无效。");
        return false;
    }
    TMap<FPrimaryAssetId, FInventoryItemDefinition> Prepared;
    Prepared.Reserve(InDefinitions.Num());
    for (const FInventoryItemDefinition& Definition : InDefinitions)
    {
        if (!ValidateDefinition(Definition, OutError))
        {
            return false;
        }
        if (Prepared.Contains(Definition.ItemId))
        {
            OutError = FString::Printf(TEXT("重复物品定义：%s。"), *Definition.ItemId.ToString());
            return false;
        }
        Prepared.Add(Definition.ItemId, Definition);
    }
    Definitions = MoveTemp(Prepared);
    bInitialized = true;
    return true;
}

bool FInventoryItemRegistry::IsInitialized() const
{
    return bInitialized;
}

const FInventoryItemDefinition* FInventoryItemRegistry::Find(const FPrimaryAssetId& InItemId) const
{
    return Definitions.Find(InItemId);
}

bool FInventoryItemRegistry::ValidateDefinition(const FInventoryItemDefinition& InDefinition, FString& OutError) const
{
    OutError.Reset();
    if (!InDefinition.ItemId.IsValid() || InDefinition.ItemId.ToString().Len() > 256
        || InDefinition.MaxStack < 1 || InDefinition.MaxLevel < 0)
    {
        OutError = TEXT("物品定义的身份、堆叠上限或等级上限无效。");
        return false;
    }
    if (InDefinition.StackPolicy != EInventoryStackPolicy::Stackable
        && InDefinition.StackPolicy != EInventoryStackPolicy::Unique)
    {
        OutError = TEXT("物品必须声明明确的堆叠策略。");
        return false;
    }
    if (InDefinition.StackPolicy == EInventoryStackPolicy::Unique && InDefinition.MaxStack != 1)
    {
        OutError = TEXT("唯一实例物品的堆叠上限必须为一。");
        return false;
    }
    if (static_cast<uint8>(InDefinition.UsePolicy) > static_cast<uint8>(EInventoryUsePolicy::QuestKey)
        || static_cast<uint8>(InDefinition.HandPolicy) > static_cast<uint8>(EInventoryHandPolicy::TwoHand)
        || InDefinition.ConsumableCount < 1 || InDefinition.PropertyRules.Num() > 16)
    {
        OutError = TEXT("物品策略、消耗量或属性数量无效。");
        return false;
    }
    if (InDefinition.UsePolicy == EInventoryUsePolicy::Equip && !InDefinition.EquipPart.IsValid())
    {
        OutError = TEXT("可装备物品必须配置装备部位。");
        return false;
    }
    if (InDefinition.UsePolicy == EInventoryUsePolicy::PlaceVoxel && !InDefinition.PlaceableVoxelId.IsValid())
    {
        OutError = TEXT("可放置物品必须配置体素定义身份。");
        return false;
    }
    TSet<FName> PropertyKeys;
    for (const FInventoryPropertyRule& Rule : InDefinition.PropertyRules)
    {
        if (Rule.Key.IsNone() || Rule.Key.ToString().Len() > 64 || PropertyKeys.Contains(Rule.Key)
            || Rule.SchemaVersion < 1 || Rule.SchemaVersion > 65535
            || Rule.Type == EInventoryPropertyType::None
            || static_cast<uint8>(Rule.Type) > static_cast<uint8>(EInventoryPropertyType::Name)
            || Rule.Minimum > Rule.Maximum || Rule.MaximumNameLength < 1 || Rule.MaximumNameLength > 64
            || Rule.AllowedNames.Num() > 256)
        {
            OutError = TEXT("物品属性白名单存在无效规则或重复键。");
            return false;
        }
        if (Rule.Key == FName(TEXT("BoundOwner")) && Rule.Type != EInventoryPropertyType::Guid)
        {
            OutError = TEXT("BoundOwner 属性必须使用 Guid 类型。");
            return false;
        }
        TSet<FName> NameValues;
        for (const FName AllowedName : Rule.AllowedNames)
        {
            if (Rule.Type != EInventoryPropertyType::Name || AllowedName.IsNone()
                || AllowedName.ToString().Len() > Rule.MaximumNameLength || NameValues.Contains(AllowedName))
            {
                OutError = TEXT("属性名称白名单无效。");
                return false;
            }
            NameValues.Add(AllowedName);
        }
        PropertyKeys.Add(Rule.Key);
    }
    return true;
}

bool FInventoryItemRegistry::ValidateItem(const FAbilityItem& InItem, FString& OutError) const
{
    OutError.Reset();
    if (!bInitialized)
    {
        OutError = TEXT("物品注册表尚未初始化。");
        return false;
    }
    if (InItem.IsEmpty())
    {
        return true;
    }
    const FInventoryItemDefinition* Definition = Find(InItem.ItemId);
    if (!Definition || InItem.Count < 1 || InItem.Count > Definition->MaxStack
        || InItem.Level < 0 || InItem.Level > Definition->MaxLevel)
    {
        OutError = TEXT("物品身份、数量或等级不符合已注册定义。");
        return false;
    }
    const bool bUnique = Definition->StackPolicy == EInventoryStackPolicy::Unique;
    if ((bUnique && (!InItem.InstanceId.IsValid() || InItem.Count != 1))
        || (!bUnique && InItem.InstanceId.IsValid()) || InItem.Properties.Num() > 16)
    {
        OutError = TEXT("物品实例身份与堆叠策略不一致。");
        return false;
    }
    TSet<FName> SeenProperties;
    FName PreviousKey;
    for (const FInventoryItemProperty& Property : InItem.Properties)
    {
        const FInventoryPropertyRule* Rule = Definition->PropertyRules.FindByPredicate([&Property](const FInventoryPropertyRule& InRule)
        {
            return InRule.Key == Property.Key;
        });
        if (!Rule || SeenProperties.Contains(Property.Key) || !Property.HasCanonicalFields()
            || Property.Type != Rule->Type || Property.SchemaVersion != Rule->SchemaVersion)
        {
            OutError = TEXT("物品属性不在白名单中，或类型、版本及规范字段无效。");
            return false;
        }
        if (!PreviousKey.IsNone() && PreviousKey.ToString().Compare(Property.Key.ToString(), ESearchCase::IgnoreCase) >= 0)
        {
            OutError = TEXT("物品属性必须按键名严格递增排列。");
            return false;
        }
        if (Property.Type == EInventoryPropertyType::Integer
            && (Property.IntValue < Rule->Minimum || Property.IntValue > Rule->Maximum))
        {
            OutError = TEXT("整数物品属性越界。");
            return false;
        }
        if (Property.Type == EInventoryPropertyType::Guid && !Property.GuidValue.IsValid())
        {
            OutError = TEXT("Guid 物品属性不能使用空身份。");
            return false;
        }
        if (Property.Type == EInventoryPropertyType::Name
            && (Property.NameValue.IsNone() || Property.NameValue.ToString().Len() > Rule->MaximumNameLength
                || (!Rule->AllowedNames.IsEmpty() && !Rule->AllowedNames.Contains(Property.NameValue))))
        {
            OutError = TEXT("名称物品属性不符合长度或白名单规则。");
            return false;
        }
        PreviousKey = Property.Key;
        SeenProperties.Add(Property.Key);
    }
    for (const FInventoryPropertyRule& Rule : Definition->PropertyRules)
    {
        if (Rule.bRequired && !SeenProperties.Contains(Rule.Key))
        {
            OutError = TEXT("物品缺少必需属性。");
            return false;
        }
    }
    return true;
}

void FInventoryItemRegistry::GetDefinitions(TArray<FInventoryItemDefinition>& OutDefinitions) const
{
    Definitions.GenerateValueArray(OutDefinitions);
    OutDefinitions.Sort([](const FInventoryItemDefinition& InLeft, const FInventoryItemDefinition& InRight)
    {
        return InLeft.ItemId.ToString().Compare(InRight.ItemId.ToString(), ESearchCase::IgnoreCase) < 0;
    });
}
