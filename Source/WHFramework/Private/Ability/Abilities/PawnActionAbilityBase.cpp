#include "Ability/Abilities/PawnActionAbilityBase.h"

#include "Ability/Pawn/AbilityPawnInterface.h"

UPawnActionAbilityBase::UPawnActionAbilityBase()
{
	bWasStopped = false;
}

void UPawnActionAbilityBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UPawnActionAbilityBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	IAbilityPawnInterface* AbilityPawn = GetOwnerActor<IAbilityPawnInterface>();

	if(!AbilityPawn) return;
	
	if(!bWasStopped)
	{
		AbilityPawn->EndAction(AbilityTags.GetByIndex(0), bWasCancelled);
	}
}
