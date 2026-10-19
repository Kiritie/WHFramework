#include "Ability/Item/AbilityItemDataBase.h"
#include "GameplayEffect.h"

#include "Ability/AbilityModule.h"
#include "Ability/AbilityModuleStatics.h"
#include "Ability/Abilities/ItemAbilityBase.h"
#include "Ability/Effects/EffectBase.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
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
	if(AbilityClass)
	{
		FString AbilityInfoStr;
		FAbilityInfo AbilityInfo = UAbilityModuleStatics::GetAbilityInfoByClass(AbilityClass, InLevel);
		for(auto& Iter1 : AbilityInfo.Effects)
		{
			for(auto& Iter2 : Iter1.Attributes)
			{
				FText AttributeName = UCommonModuleStatics::GetPropertyDisplayName(Iter2.Attribute.GetUProperty());
				if(Iter2.BaseAttribute.IsValid())
				{
					FText BaseAttributeName = UCommonModuleStatics::GetPropertyDisplayName(Iter2.BaseAttribute.GetUProperty());
					AbilityInfoStr += (Iter2.AttributeSource == EGameplayEffectAttributeCaptureSource::Source ? FText::Format(NSLOCTEXT("WH.AbilityItem", "SourceAttribute", "{0}: {1}%自身{2}\n"), AttributeName, FText::AsNumber((int32)(Iter2.Value * 100)), BaseAttributeName) : FText::Format(NSLOCTEXT("WH.AbilityItem", "TargetAttribute", "{0}: {1}%目标{2}\n"), AttributeName, FText::AsNumber((int32)(Iter2.Value * 100)), BaseAttributeName)).ToString();
				}
				else
				{
					AbilityInfoStr.Appendf(TEXT("%s: %s%s\n"), *AttributeName.ToString(), (Iter2.Attribute.GetName().EndsWith(TEXT("Rate")) ? *FString::Printf(TEXT("%s%%"), *UCommonModuleStatics::SanitizeFloat(Iter2.Value * 100, 2)) : *UCommonModuleStatics::SanitizeFloat(Iter2.Value, 2)),
						(Iter2.Attribute.GetName().EndsWith(TEXT("Time")) || Iter2.Attribute.GetName().EndsWith(TEXT("Interrupt")) ? TEXT("s") : TEXT("")));
				}
			}
			if(Iter1.Period > 0.f)
			{
				AbilityInfoStr += FText::Format(NSLOCTEXT("WH.AbilityItem", "Period", "触发周期: {0}s\n"), FText::FromString(UCommonModuleStatics::SanitizeFloat(Iter1.Period, 2))).ToString();
			}
			if(Iter1.Duration > 0.f)
			{
				AbilityInfoStr += FText::Format(NSLOCTEXT("WH.AbilityItem", "Duration", "持续时间: {0}s\n"), FText::FromString(UCommonModuleStatics::SanitizeFloat(Iter1.Duration, 2))).ToString();
			}
		}
		if(AbilityInfo.CooldownDuration > 0.f)
		{
			AbilityInfoStr += FText::Format(NSLOCTEXT("WH.AbilityItem", "Cooldown", "冷却时间: {0}s\n"), FText::FromString(UCommonModuleStatics::SanitizeFloat(AbilityInfo.CooldownDuration, 2))).ToString();
		}
		if(FMath::Abs(AbilityInfo.CostValue) > 0.f)
		{
			FText AttributeName = UCommonModuleStatics::GetPropertyDisplayName(AbilityInfo.CostAttribute.GetUProperty());
			AbilityInfoStr += FText::Format(NSLOCTEXT("WH.AbilityItem", "Cost", "消耗: {0}{1}\n"), FText::FromString(UCommonModuleStatics::SanitizeFloat(FMath::Abs(AbilityInfo.CostValue), 2)), AttributeName).ToString();
		}
		
		AbilityInfoStr.RemoveFromEnd(TEXT("\n"));
		
		return AbilityInfoStr;
	}
	return TEXT("");
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
    const float Level = FMath::Max(1, InLevel);
    TArray<FAbilityItemAttributeValue> Values;
    for (TSubclassOf<UEffectBase> EffectType : Effects)
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
