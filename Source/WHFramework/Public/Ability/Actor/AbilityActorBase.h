// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "AbilityActorInterface.h"

#include "AbilityActorBase.generated.h"

class UAttributeSetBase;
class UAbilitySystemComponentBase;
/**
 * Ability Actor基类
 */
UCLASS()
class WHFRAMEWORK_API AAbilityActorBase : public AWHActor, public IAbilityActorInterface, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AAbilityActorBase();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAbilitySystemComponentBase* AbilitySystem;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAttributeSetBase* AttributeSet;

protected:
	virtual void BeginPlay() override;

public:
	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation() override;

	virtual void Tick(float DeltaTime) override;

	virtual void Serialize(FArchive& Ar) override;

	//////////////////////////////////////////////////////////////////////////
	/// Ability
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "ActorStats")
	int32 Level;

public:
	UFUNCTION(BlueprintPure)
	virtual int32 GetLevelV() const override { return Level; }

	UFUNCTION(BlueprintCallable)
	virtual void SetLevelV(int32 InLevel) override { Level = InLevel; }

public:
	UFUNCTION(BlueprintCallable)
	virtual FGameplayAbilitySpecHandle AcquireAbility(TSubclassOf<UAbilityBase> InAbility, int32 InLevel = 1) override;

	UFUNCTION(BlueprintCallable)
	virtual bool ActiveAbility(FGameplayAbilitySpecHandle AbilityHandle, bool bAllowRemoteActivation = false) override;

	UFUNCTION(BlueprintCallable)
	virtual bool ActiveAbilityByClass(TSubclassOf<UAbilityBase> AbilityClass, bool bAllowRemoteActivation = false) override;

	UFUNCTION(BlueprintCallable)
	virtual bool ActiveAbilityByTag(const FGameplayTagContainer& GameplayTagContainer, bool bAllowRemoteActivation = false) override;

	UFUNCTION(BlueprintCallable)
	virtual void CancelAbility(UAbilityBase* Ability) override;

	UFUNCTION(BlueprintCallable)
	virtual void CancelAbilityByHandle(const FGameplayAbilitySpecHandle& AbilityHandle) override;

	UFUNCTION(BlueprintCallable)
	virtual void CancelAbilities(const FGameplayTagContainer& WithTags, const FGameplayTagContainer& WithoutTags, UAbilityBase* Ignore=nullptr) override;
	
	UFUNCTION(BlueprintCallable)
	virtual void CancelAllAbilities(UAbilityBase* Ignore=nullptr) override;
	
	UFUNCTION(BlueprintCallable)
	virtual FActiveGameplayEffectHandle ApplyEffectByClass(TSubclassOf<UGameplayEffect> EffectClass) override;
	
	UFUNCTION(BlueprintCallable)
	virtual FActiveGameplayEffectHandle ApplyEffectBySpecHandle(const FGameplayEffectSpecHandle& SpecHandle) override;
		
	UFUNCTION(BlueprintCallable)
	virtual FActiveGameplayEffectHandle ApplyEffectBySpec(const FGameplayEffectSpec& Spec) override;

	UFUNCTION(BlueprintCallable)
	virtual bool RemoveEffect(FActiveGameplayEffectHandle Handle, int32 StacksToRemove=-1) override;

	UFUNCTION(BlueprintPure)
	virtual void GetActiveAbilities(FGameplayTagContainer AbilityTags, TArray<UAbilityBase*>& ActiveAbilities) override;

	UFUNCTION(BlueprintPure)
	virtual bool GetAbilityInfo(TSubclassOf<UAbilityBase> AbilityClass, FAbilityInfo& OutAbilityInfo) override;

public:
	virtual void OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData) override;

public:
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	template<class T>
	T* GetAttributeSet() const
	{
		return Cast<T>(AttributeSet);
	}
	UFUNCTION(BlueprintPure)
	UAttributeSetBase* GetAttributeSet() const { return AttributeSet; }
};
