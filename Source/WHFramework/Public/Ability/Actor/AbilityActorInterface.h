#pragma once

#include "Ability/AbilityModuleTypes.h"

#include "AbilityActorInterface.generated.h"

class UAbilityBase;
UINTERFACE()
class WHFRAMEWORK_API UAbilityActorInterface : public UInterface
{
	GENERATED_BODY()
};

class WHFRAMEWORK_API IAbilityActorInterface
{
	GENERATED_BODY()

public:
	virtual int32 GetLevelV() const = 0;

	virtual void SetLevelV(int32 InLevel) = 0;

public:
	virtual FGameplayAbilitySpecHandle AcquireAbility(TSubclassOf<UAbilityBase> InAbility, int32 InLevel = 1) = 0;

	virtual bool ActiveAbility(FGameplayAbilitySpecHandle AbilityHandle, bool bAllowRemoteActivation = false) = 0;

	virtual bool ActiveAbilityByClass(TSubclassOf<UAbilityBase> AbilityClass, bool bAllowRemoteActivation = false) = 0;

	virtual bool ActiveAbilityByTag(const FGameplayTagContainer& GameplayTagContainer, bool bAllowRemoteActivation = false) = 0;

	virtual void CancelAbility(UAbilityBase* Ability) = 0;

	virtual void CancelAbilityByHandle(const FGameplayAbilitySpecHandle& AbilityHandle) = 0;

	virtual void CancelAbilities(const FGameplayTagContainer& WithTags, const FGameplayTagContainer& WithoutTags, UAbilityBase* Ignore = nullptr) = 0;
	
	virtual void CancelAllAbilities(UAbilityBase* Ignore = nullptr) = 0;
	
	virtual FActiveGameplayEffectHandle ApplyEffectByClass(TSubclassOf<UGameplayEffect> EffectClass) = 0;
	
	virtual FActiveGameplayEffectHandle ApplyEffectBySpecHandle(const FGameplayEffectSpecHandle& SpecHandle) = 0;
		
	virtual FActiveGameplayEffectHandle ApplyEffectBySpec(const FGameplayEffectSpec& Spec) = 0;

	virtual bool RemoveEffect(FActiveGameplayEffectHandle Handle, int32 StacksToRemove=-1) = 0;

	virtual void GetActiveAbilities(FGameplayTagContainer AbilityTags, TArray<UAbilityBase*>& ActiveAbilities) = 0;

	virtual bool GetAbilityInfo(TSubclassOf<UAbilityBase> AbilityClass, FAbilityInfo& OutAbilityInfo) = 0;

protected:
	virtual void OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData) = 0;
};