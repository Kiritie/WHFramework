#include "Ability/Item/AbilityItemDataBase.h"
#include "GameplayEffect.h"
#include "Ability/Inventory/InventoryPickupActor.h"

#include "Ability/Abilities/ItemAbilityBase.h"
#include "Ability/Effects/EffectBase.h"
#include "Common/CommonModuleStatics.h"

UAbilityItemDataBase::UAbilityItemDataBase()
{
	Type = FName("Misc");

	Name = FText::GetEmpty();
	Detail = FText::GetEmpty();
	Rarity = EAbilityItemRarity::None;
	Icon = nullptr;
	MaxCount = 1;
	MaxLevel = 1;
	AbilityClass = nullptr;
	PickUpClass = nullptr;

	_Icon = nullptr;
}

void UAbilityItemDataBase::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	_Icon = Icon;
}

void UAbilityItemDataBase::OnReset_Implementation()
{
	Super::OnReset_Implementation();

	Icon = _Icon;
}

int32 UAbilityItemDataBase::ClampCount(int32 InCount) const
{
	return FMath::Clamp(InCount, 0, MaxCount);
}

int32 UAbilityItemDataBase::ClampLevel(int32 InLevel) const
{
	return FMath::Clamp(InLevel, 0, MaxLevel);
}

EAbilityItemType UAbilityItemDataBase::GetItemType() const
{
	return (EAbilityItemType)UCommonModuleStatics::GetEnumValueByAuthoredName(TEXT("/Script/WHFramework.EAbilityItemType"), Type.ToString());
}

FString UAbilityItemDataBase::GetItemAbilityInfo(int32 InLevel) const
{
    if (InLevel < 1 || InLevel > MaxLevel)
    {
        return FString();
    }
    TArray<FString> Lines;
    const bool bEquipment = BuildInventoryDefinition().UsePolicy == EInventoryUsePolicy::Equip;
    const auto AppendEffect = [&Lines, InLevel](const UGameplayEffect* InEffect, const FText& InHeading)
    {
        if (!InEffect)
        {
            Lines.Add(NSLOCTEXT("WH.AbilityItem", "MissingEffect", "效果配置暂不可用。").ToString());
            return;
        }
        if (!InHeading.IsEmpty())
        {
            Lines.Add(InHeading.ToString());
        }
        for (const FGameplayModifierInfo& Modifier : InEffect->Modifiers)
        {
            if (!Modifier.Attribute.IsValid())
            {
                Lines.Add(NSLOCTEXT("WH.AbilityItem", "InvalidAttribute", "效果属性暂无法确定。").ToString());
                continue;
            }
            FAbilityItemAttributeValue Value;
            Value.Attribute = Modifier.Attribute;
            Value.ModifierOp = Modifier.ModifierOp;
            Value.Channel = Modifier.EvaluationChannelSettings.GetEvaluationChannel();
            Value.bConditional = !Modifier.SourceTags.IsEmpty() || !Modifier.TargetTags.IsEmpty();
            const bool bKnown = Modifier.ModifierMagnitude.GetStaticMagnitudeIfPossible(InLevel, Value.Value) && FMath::IsFinite(Value.Value);
            Lines.Add(FText::Format(NSLOCTEXT("WH.AbilityItem", "EffectAttribute", "{0}：{1}"), FormatAttributeName(Value),
                bKnown ? FormatAttributeValue(Value, Value.Value, false)
                    : NSLOCTEXT("WH.AbilityItem", "DynamicMagnitude", "数值取决于角色、目标或使用条件")).ToString());
        }
        if (!InEffect->Executions.IsEmpty())
        {
            Lines.Add(NSLOCTEXT("WH.AbilityItem", "DynamicExecution", "附加效果需结合角色与目标状态计算。").ToString());
        }
        const float Period = InEffect->Period.GetValueAtLevel(InLevel);
        if (FMath::IsFinite(Period) && Period > 0.f)
        {
            Lines.Add(FText::Format(NSLOCTEXT("WH.AbilityItem", "EffectPeriod", "触发周期：{0}秒"), FText::AsNumber(Period)).ToString());
        }
        if (InEffect->DurationPolicy == EGameplayEffectDurationType::Infinite)
        {
            Lines.Add(NSLOCTEXT("WH.AbilityItem", "InfiniteEffect", "无固定持续时长。").ToString());
        }
        else if (InEffect->DurationPolicy == EGameplayEffectDurationType::HasDuration)
        {
            float Duration = 0.f;
            const bool bKnown = InEffect->DurationMagnitude.GetStaticMagnitudeIfPossible(InLevel, Duration) && FMath::IsFinite(Duration) && Duration > 0.f;
            Lines.Add(bKnown ? FText::Format(NSLOCTEXT("WH.AbilityItem", "EffectDuration", "持续时间：{0}秒"), FText::AsNumber(Duration)).ToString()
                : NSLOCTEXT("WH.AbilityItem", "DynamicDuration", "持续时间取决于使用时状态。").ToString());
        }
    };
    const UAbilityBase* Ability = AbilityClass.GetDefaultObject();
    if (Ability)
    {
        if (const UGameplayEffect* Cost = Ability->GetCostGameplayEffect(); Cost && !bEquipment)
        {
            AppendEffect(Cost, NSLOCTEXT("WH.AbilityItem", "CostEffects", "施放消耗："));
        }
        for (TSubclassOf<UEffectBase> EffectClass : Ability->GetEffectClasses())
        {
            const UEffectBase* Effect = EffectClass.GetDefaultObject();
            AppendEffect(Effect, FText::GetEmpty());
        }
        if (const UGameplayEffect* Cooldown = Ability->GetCooldownGameplayEffect(); Cooldown && !bEquipment)
        {
            AppendEffect(Cooldown, NSLOCTEXT("WH.AbilityItem", "CooldownEffects", "冷却效果："));
        }
    }
    if (!PassiveEffects.IsEmpty() && bEquipment)
    {
        for (TSubclassOf<UGameplayEffect> EffectClass : PassiveEffects)
        {
            const UGameplayEffect* Effect = EffectClass.GetDefaultObject();
            AppendEffect(Effect, NSLOCTEXT("WH.AbilityItem", "EquipmentEffects", "装配加成："));
        }
    }
    return FString::Join(Lines, TEXT("\n"));
}
FString UAbilityItemDataBase::GetItemErrorInfo(FAbilityItem InItem, int32 InActorLevel) const
{
    if (InActorLevel < InItem.Level)
    {
        return FText::Format(NSLOCTEXT("WH.AbilityItem", "RequiredLevel", "角色未达到[{0}]级"), FText::AsNumber(InItem.Level)).ToString();
    }
    return FString();
}

FInventoryItemDefinition UAbilityItemDataBase::BuildInventoryDefinition() const
{
    FInventoryItemDefinition Definition;
    Definition.ItemId = GetPrimaryAssetId();
    Definition.DisplayName = Name;
    Definition.Icon = FSoftObjectPath(Icon);
    Definition.Tags = InventoryTags;
    Definition.MaxStack = MaxCount;
    Definition.MaxLevel = MaxLevel;
    Definition.StackPolicy = StackPolicy;
    Definition.UsePolicy = UsePolicy;
    Definition.AbilityClass = FSoftClassPath(AbilityClass.Get());
    Definition.PickupActorClass = FSoftClassPath(PickUpClass.Get());
    Definition.ConsumableCount = ConsumableCount;
    Definition.bCanTrade = bCanTrade;
    Definition.bCanDrop = bCanDrop;
    Definition.PropertyRules = PropertyRules;
    for (const TSubclassOf<UGameplayEffect>& Effect : PassiveEffects)
    {
        Definition.PassiveEffects.Add(FSoftClassPath(Effect.Get()));
    }
    return Definition;
}

bool UAbilityItemDataBase::GetEquipmentAttributeValues(const FInventoryItemDefinition& InDefinition, int32 InLevel, TArray<FAbilityItemAttributeValue>& OutValues)
{
    OutValues.Reset();
    if (InDefinition.UsePolicy != EInventoryUsePolicy::Equip)
    {
        return false;
    }
    TArray<TSubclassOf<UEffectBase>> Effects;
    if (!InDefinition.AbilityClass.IsNull())
    {
        const UClass* AbilityType = InDefinition.AbilityClass.ResolveClass();
        const UItemAbilityBase* Ability = AbilityType ? Cast<UItemAbilityBase>(AbilityType->GetDefaultObject()) : nullptr;
        if (!Ability)
        {
            return false;
        }
        for (TSubclassOf<UEffectBase> Effect : Ability->GetEffectClasses())
        {
            Effects.AddUnique(Effect);
        }
    }
    for (const FSoftClassPath& Path : InDefinition.PassiveEffects)
    {
        UClass* EffectType = Path.ResolveClass();
        if (!EffectType || !EffectType->IsChildOf(UEffectBase::StaticClass()))
        {
            return false;
        }
        Effects.AddUnique(EffectType);
    }
    return GetEffectAttributeValues(Effects, InLevel, OutValues);
}

bool UAbilityItemDataBase::GetEffectAttributeValues(const TArray<TSubclassOf<UEffectBase>>& InEffects, int32 InLevel, TArray<FAbilityItemAttributeValue>& OutValues)
{
    OutValues.Reset();
    const float Level = FMath::Max(1, InLevel);
    TArray<FAbilityItemAttributeValue> Values;
    for (TSubclassOf<UEffectBase> EffectType : InEffects)
    {
        const UEffectBase* Effect = EffectType.GetDefaultObject();
        if (!Effect || !Effect->SupportsAttributeGrant(Level))
        {
            return false;
        }
        const FGameplayEffectSpec Spec(Effect, FGameplayEffectContextHandle(), Level);
        for (const FGameplayModifierInfo& Modifier : Effect->Modifiers)
        {
            FAbilityItemAttributeValue Value;
            Value.Attribute = Modifier.Attribute;
            Value.ModifierOp = Modifier.ModifierOp;
            Value.Channel = Modifier.EvaluationChannelSettings.GetEvaluationChannel();
            Value.bConditional = !Modifier.SourceTags.IsEmpty() || !Modifier.TargetTags.IsEmpty();
            if (!Modifier.ModifierMagnitude.AttemptCalculateMagnitude(Spec, Value.Value, false, 0.f) || !FMath::IsFinite(Value.Value))
            {
                return false;
            }
            FAbilityItemAttributeValue* Existing = !Value.bConditional && Value.ModifierOp == EGameplayModOp::Additive
                ? Values.FindByPredicate([&Value](const FAbilityItemAttributeValue& Entry)
                {
                    return !Entry.bConditional && Entry.Attribute == Value.Attribute && Entry.ModifierOp == Value.ModifierOp && Entry.Channel == Value.Channel;
                }) : nullptr;
            if (Existing)
            {
                Existing->Value += Value.Value;
                if (!FMath::IsFinite(Existing->Value))
                {
                    return false;
                }
            }
            else
            {
                Values.Add(Value);
            }
        }
    }
    OutValues = MoveTemp(Values);
    return true;
}

FText UAbilityItemDataBase::FormatAttributeName(const FAbilityItemAttributeValue& InValue)
{
    FText AttributeName = UCommonModuleStatics::GetPropertyDisplayName(InValue.Attribute.GetUProperty());
    FText Operation;
    switch (InValue.ModifierOp)
    {
        case EGameplayModOp::MultiplyAdditive: Operation = NSLOCTEXT("WH.AbilityItem", "MultiplyAdditive", "乘数"); break;
        case EGameplayModOp::DivideAdditive: Operation = NSLOCTEXT("WH.AbilityItem", "DivideAdditive", "除数"); break;
        case EGameplayModOp::MultiplyCompound: Operation = NSLOCTEXT("WH.AbilityItem", "MultiplyCompound", "连乘"); break;
        case EGameplayModOp::Override: Operation = NSLOCTEXT("WH.AbilityItem", "Override", "覆盖"); break;
        case EGameplayModOp::AddFinal: Operation = NSLOCTEXT("WH.AbilityItem", "AddFinal", "最终加成"); break;
        default: Operation = NSLOCTEXT("WH.AbilityItem", "AddBase", "加成"); break;
    }
    AttributeName = FText::Format(NSLOCTEXT("WH.AbilityItem", "AttributeOperation", "{0}·{1}"), AttributeName, Operation);
    if (InValue.bConditional)
    {
        AttributeName = FText::Format(NSLOCTEXT("WH.AbilityItem", "ConditionalAttribute", "{0}（条件）"), AttributeName);
    }
    if (InValue.Channel != EGameplayModEvaluationChannel::Channel0)
    {
        AttributeName = FText::Format(NSLOCTEXT("WH.AbilityItem", "ChannelAttribute", "{0}（通道{1}）"), AttributeName, FText::AsNumber(static_cast<int32>(InValue.Channel)));
    }
    return AttributeName;
}

FText UAbilityItemDataBase::FormatAttributeValue(const FAbilityItemAttributeValue& InValue, float InMagnitude, bool bInSigned)
{
    const bool bRate = (InValue.ModifierOp == EGameplayModOp::AddBase || InValue.ModifierOp == EGameplayModOp::AddFinal) && InValue.Attribute.GetName().EndsWith(TEXT("Rate"));
    FNumberFormattingOptions Options;
    Options.AlwaysSign = bInSigned;
    Options.MaximumFractionalDigits = 4;
    const FText Number = FText::AsNumber(bRate ? InMagnitude * 100.0 : InMagnitude, &Options);
    if (bRate)
    {
        return FText::Format(NSLOCTEXT("WH.AbilityItem", "RateValue", "{0}%"), Number);
    }
    if ((InValue.ModifierOp == EGameplayModOp::AddBase || InValue.ModifierOp == EGameplayModOp::AddFinal) &&
        (InValue.Attribute.GetName().EndsWith(TEXT("Time")) || InValue.Attribute.GetName().EndsWith(TEXT("Interrupt"))))
    {
        return FText::Format(NSLOCTEXT("WH.AbilityItem", "TimeValue", "{0}s"), Number);
    }
    return Number;
}
