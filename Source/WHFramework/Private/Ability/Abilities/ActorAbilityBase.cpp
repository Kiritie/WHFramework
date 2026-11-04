
#include "Ability/Abilities/ActorAbilityBase.h"

UActorAbilityBase::UActorAbilityBase()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActionTag = AbilityActionTags::ItemUse;
	bActivateOnInput = false;
}
