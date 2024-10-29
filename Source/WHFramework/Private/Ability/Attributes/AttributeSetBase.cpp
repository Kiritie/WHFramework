#include "Ability/Attributes/AttributeSetBase.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemComponent.h"
#include "Ability/Character/AbilityCharacterBase.h"

UAttributeSetBase::UAttributeSetBase()
{
}

void UAttributeSetBase::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
}

void UAttributeSetBase::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
}

void UAttributeSetBase::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
}

void UAttributeSetBase::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
}

void UAttributeSetBase::AdjustAttributeForMaxChange(const FGameplayAttribute& Attribute, float OldMaxValue, float NewMaxValue)
{
	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	if(!FMath::IsNearlyEqual(OldMaxValue, NewMaxValue) && AbilityComp)
	{
		const float CurrentValue = Attribute.GetGameplayAttributeData(this)->GetCurrentValue();
		float NewDelta = CurrentValue / OldMaxValue * NewMaxValue - CurrentValue;
		ModifyAttributeValue(Attribute, NewDelta);
	}
}

FGameplayAttributeData UAttributeSetBase::GetAttributeData(FGameplayAttribute InAttribute)
{
	return *InAttribute.GetGameplayAttributeData(this);
}

float UAttributeSetBase::GetAttributeValue(FGameplayAttribute InAttribute)
{
	return GetAttributeData(InAttribute).GetCurrentValue();
}

float UAttributeSetBase::GetAttributeBaseValue(FGameplayAttribute InAttribute)
{
	return GetAttributeData(InAttribute).GetBaseValue();
}

void UAttributeSetBase::SetAttributeValue(FGameplayAttribute InAttribute, float InValue)
{
	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	
	AbilityComp->ApplyModToAttributeUnsafe(InAttribute, EGameplayModOp::Override, InValue);
}

void UAttributeSetBase::ModifyAttributeValue(FGameplayAttribute InAttribute, float InDeltaValue)
{
	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	
	AbilityComp->ApplyModToAttributeUnsafe(InAttribute, EGameplayModOp::Additive, InDeltaValue);
}

void UAttributeSetBase::MultipleAttributeValue(FGameplayAttribute InAttribute, float InMultipleValue)
{
	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	
	AbilityComp->ApplyModToAttributeUnsafe(InAttribute, EGameplayModOp::Multiplicitive, InMultipleValue);
}

TArray<FGameplayAttribute> UAttributeSetBase::GetAllAttributes() const
{
	TArray<FGameplayAttribute> AllAttributes;
	for(TFieldIterator<FProperty> PropertyIt(GetClass(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		FStructProperty* Property = CastField<FStructProperty>(*PropertyIt);
		if(Property && Property->Struct == FGameplayAttributeData::StaticStruct())
		{
			AllAttributes.Add(Property);
		}
	}
	return AllAttributes;
}

TArray<FGameplayAttribute> UAttributeSetBase::GetPersistentAttributes() const
{
	TArray<FGameplayAttribute> PersistentAttributes;
	for(TFieldIterator<FProperty> PropertyIt(GetClass(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		FStructProperty* Property = CastField<FStructProperty>(*PropertyIt);
		if(Property && Property->Struct == FGameplayAttributeData::StaticStruct() && Property->HasAnyPropertyFlags(CPF_SaveGame))
		{
			PersistentAttributes.Add(Property);
		}
	}
	return PersistentAttributes;
}
