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

void UAttributeSetBase::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
}

void UAttributeSetBase::AdjustAttributeForMaxChange(FGameplayAttributeData& InAffectedAttribute, const FGameplayAttributeData& InMaxAttribute, float InNewMaxValue, const FGameplayAttribute& InAffectedAttributeProperty)
{
	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	const float CurrentMaxValue = InMaxAttribute.GetCurrentValue();
	if(!FMath::IsNearlyEqual(CurrentMaxValue, InNewMaxValue) && AbilityComp)
	{
		const float CurrentValue = InAffectedAttribute.GetCurrentValue();
		float NewDelta = CurrentMaxValue > 0.f ? ((CurrentValue / CurrentMaxValue) * InNewMaxValue - CurrentValue) : InNewMaxValue;
		ModifyAttributeValue(InAffectedAttributeProperty, NewDelta);
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
	IAbilityActorInterface* AbilityActor = Cast<IAbilityActorInterface>(AbilityComp->GetAvatarActor());
	
	FOnAttributeChangeData OnAttributeChangeData;
	OnAttributeChangeData.Attribute = InAttribute;
	OnAttributeChangeData.OldValue = GetAttributeValue(InAttribute);
	
	AbilityComp->ApplyModToAttributeUnsafe(InAttribute, EGameplayModOp::Override, InValue);
	OnAttributeChangeData.NewValue = GetAttributeValue(InAttribute);

	if(OnAttributeChangeData.NewValue != OnAttributeChangeData.OldValue)
	{
		//AbilityActor->OnAttributeChange(OnAttributeChangeData);
	}
}

void UAttributeSetBase::ModifyAttributeValue(FGameplayAttribute InAttribute, float InDeltaValue)
{
	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	IAbilityActorInterface* AbilityActor = Cast<IAbilityActorInterface>(AbilityComp->GetAvatarActor());
	
	FOnAttributeChangeData OnAttributeChangeData;
	OnAttributeChangeData.Attribute = InAttribute;
	OnAttributeChangeData.OldValue = GetAttributeValue(InAttribute);
	
	AbilityComp->ApplyModToAttributeUnsafe(InAttribute, EGameplayModOp::Additive, InDeltaValue);
	OnAttributeChangeData.NewValue = GetAttributeValue(InAttribute);

	if(OnAttributeChangeData.NewValue != OnAttributeChangeData.OldValue)
	{
		//AbilityActor->OnAttributeChange(OnAttributeChangeData);
	}
}

void UAttributeSetBase::MultipleAttributeValue(FGameplayAttribute InAttribute, float InMultipleValue)
{
	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	IAbilityActorInterface* AbilityActor = Cast<IAbilityActorInterface>(AbilityComp->GetAvatarActor());
	
	FOnAttributeChangeData OnAttributeChangeData;
	OnAttributeChangeData.Attribute = InAttribute;
	OnAttributeChangeData.OldValue = GetAttributeValue(InAttribute);
	
	AbilityComp->ApplyModToAttributeUnsafe(InAttribute, EGameplayModOp::Multiplicitive, InMultipleValue);
	OnAttributeChangeData.NewValue = GetAttributeValue(InAttribute);

	if(OnAttributeChangeData.NewValue != OnAttributeChangeData.OldValue)
	{
		//AbilityActor->OnAttributeChange(OnAttributeChangeData);
	}
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
