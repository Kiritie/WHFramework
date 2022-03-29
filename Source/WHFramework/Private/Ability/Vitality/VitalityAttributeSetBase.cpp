#include "Ability/Vitality/VitalityAttributeSetBase.h"

#include "GameplayEffectExtension.h"

UVitalityAttributeSetBase::UVitalityAttributeSetBase()
{
	
}

void UVitalityAttributeSetBase::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
}

void UVitalityAttributeSetBase::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
}
