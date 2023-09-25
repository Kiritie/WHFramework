#include "Ability/Attributes/PawnAttributeSetBase.h"

#include "GameplayEffectExtension.h"
#include "Ability/Pawn/AbilityPawnBase.h"

UPawnAttributeSetBase::UPawnAttributeSetBase()
{
	
}

void UPawnAttributeSetBase::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
}

void UPawnAttributeSetBase::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
}

void UPawnAttributeSetBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

