#include "Ability/Base/AttributeSetBase.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemComponent.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Vitality/AbilityVitalityInterface.h"
#include "GameFramework/PlayerController.h"

UAttributeSetBase::UAttributeSetBase()
:	Health(100.f),
	MaxHealth(100.f)
{
	
}

void UAttributeSetBase::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	IAbilityVitalityInterface* TargetVitality = Cast<IAbilityVitalityInterface>(AbilityComp->GetAvatarActor());
	
	const float CurrentValue = Attribute.GetGameplayAttributeData(this)->GetCurrentValue();
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
		if (TargetVitality)
		{
			TargetVitality->HandleHealthChanged(NewValue, NewValue - CurrentValue);
		}
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		if (TargetVitality)
		{
			TargetVitality->HandleMaxHealthChanged(NewValue, NewValue - CurrentValue);
		}
		AdjustAttributeForMaxChange(Health, MaxHealth, NewValue, GetHealthAttribute());
	}
}

void UAttributeSetBase::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
}

void UAttributeSetBase::AdjustAttributeForMaxChange(FGameplayAttributeData& AffectedAttribute, const FGameplayAttributeData& MaxAttribute, float NewMaxValue, const FGameplayAttribute& AffectedAttributeProperty)
{
	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	const float CurrentMaxValue = MaxAttribute.GetCurrentValue();
	if (!FMath::IsNearlyEqual(CurrentMaxValue, NewMaxValue) && AbilityComp)
	{
		const float CurrentValue = AffectedAttribute.GetCurrentValue();
		float NewDelta = CurrentMaxValue > 0.f ? (CurrentValue / CurrentMaxValue * NewMaxValue - CurrentValue) : NewMaxValue;
		AbilityComp->ApplyModToAttributeUnsafe(AffectedAttributeProperty, EGameplayModOp::Additive, NewDelta);
	}
}

FGameplayAttributeData* UAttributeSetBase::GetAttributeData(const FGameplayAttribute& Attribute)
{
	return Attribute.GetGameplayAttributeData(this);
}
