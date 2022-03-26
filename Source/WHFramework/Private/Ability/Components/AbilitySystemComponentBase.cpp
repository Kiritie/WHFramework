#include "Ability/Components/AbilitySystemComponentBase.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemGlobals.h"
#include "GameplayAbilitySpec.h"
#include "Ability/Base/AbilityBase.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "Character/Base/CharacterBase.h"

UAbilitySystemComponentBase::UAbilitySystemComponentBase() { }

void UAbilitySystemComponentBase::GetActiveAbilitiesWithTags(const FGameplayTagContainer& AbilityTags, TArray<UAbilityBase*>& ActiveAbilities)
{
	// 获取标签对应的已激活的Ability列表
	TArray<FGameplayAbilitySpec*> AbilitiesToActivate;
	GetActivatableGameplayAbilitySpecsByAllMatchingTags(AbilityTags, AbilitiesToActivate, false);

	for (FGameplayAbilitySpec* Spec : AbilitiesToActivate)
	{
		// 获取Ability实例列表
		TArray<UGameplayAbility*> AbilityInstances = Spec->GetAbilityInstances();

		for (UGameplayAbility* ActiveAbility : AbilityInstances)
		{
			// 将Ability实例加入返回列表中
			ActiveAbilities.Add(Cast<UAbilityBase>(ActiveAbility));
		}
	}
}

int32 UAbilitySystemComponentBase::GetDefaultAbilityLevel() const
{
	AAbilityCharacterBase* OwnerCharacter = Cast<AAbilityCharacterBase>(GetOwnerActor());
	if (OwnerCharacter)
	{
		return OwnerCharacter->GetLevelV();
	}
	return 1;
}

UAbilitySystemComponentBase* UAbilitySystemComponentBase::GetAbilitySystemComponentFormActor(const AActor* Actor, bool LookForComponent /*= false*/)
{
	return Cast<UAbilitySystemComponentBase>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor, LookForComponent));
}
