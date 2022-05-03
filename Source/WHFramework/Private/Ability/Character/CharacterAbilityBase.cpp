#include "Ability/Character/CharacterAbilityBase.h"
#include "Ability/Character/AbilityCharacterBase.h"

UCharacterAbilityBase::UCharacterAbilityBase()
{
	AnimMontage = nullptr;
	bAutoEndAbility = true;
}

AAbilityCharacterBase* UCharacterAbilityBase::GetOwnerCharacter(TSubclassOf<AAbilityCharacterBase> InOwnerClass) const
{
	return GetOwnerCharacter<AAbilityCharacterBase>();
}
