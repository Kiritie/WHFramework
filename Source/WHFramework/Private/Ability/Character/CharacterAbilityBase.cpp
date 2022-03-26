#include "Ability/Character/CharacterAbilityBase.h"
#include "Ability/Character/AbilityCharacterBase.h"

UCharacterAbilityBase::UCharacterAbilityBase()
{
	AnimMontage = nullptr;
	bAutoEndAbility = true;
}

AAbilityCharacterBase* UCharacterAbilityBase::GetOwnerCharacter() const
{
	return Cast<AAbilityCharacterBase>(GetOwningActorFromActorInfo());
}
