#include "Ability/Vitality/VitalityAttributeSetBase.h"

#include "GameplayEffectExtension.h"
#include "Ability/Vitality/AbilityVitalityInterface.h"

UVitalityAttributeSetBase::UVitalityAttributeSetBase()
:	Health(100.f),
	MaxHealth(100.f)
{
}

void UVitalityAttributeSetBase::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

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

void UVitalityAttributeSetBase::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
}
