#pragma once

#include "AbilitySystemInterface.h"
#include "Ability/AbilityModuleTypes.h"

#include "AbilityActorInterface.generated.h"

class UAttributeSetBase;
class UAbilityBase;

UINTERFACE()
class WHFRAMEWORK_API UAbilityActorInterface : public UAbilitySystemInterface
{
	GENERATED_BODY()
};

class WHFRAMEWORK_API IAbilityActorInterface : public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	virtual int32 GetLevelV() const = 0;

	virtual void SetLevelV(int32 InLevel) = 0;

public:
	virtual void InitializeAbilitySystem();
	
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
	
	virtual FGameplayAbilitySpec GetAbilitySpecByHandle(FGameplayAbilitySpecHandle Handle) = 0;
	
	virtual FGameplayAbilitySpec GetAbilitySpecByGEHandle(FActiveGameplayEffectHandle Handle) = 0;

	virtual FGameplayAbilitySpec GetAbilitySpecByClass(TSubclassOf<UGameplayAbility> InAbilityClass) = 0;

	virtual FGameplayAbilitySpec GetAbilitySpecByInputID(int32 InputID) = 0;

public:
	virtual void RefreshAttributes();
	
	virtual void OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData) = 0;
	
	virtual UAttributeSetBase* GetAttributeSet() const = 0;

	virtual FGameplayAttributeData GetAttributeData(FGameplayAttribute InAttribute) = 0;

	virtual float GetAttributeValue(FGameplayAttribute InAttribute) = 0;

	virtual void SetAttributeValue(FGameplayAttribute InAttribute, float InValue) = 0;

	virtual TArray<FGameplayAttribute> GetAllAttributes() const = 0;

	virtual TArray<FGameplayAttribute> GetPersistentAttributes() const = 0;
};