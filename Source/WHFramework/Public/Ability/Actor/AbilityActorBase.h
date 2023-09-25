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
class WHFRAMEWORK_API AAbilityActorBase : public AWHActor, public IAbilityActorInterface
{
	GENERATED_BODY()

public:
	AAbilityActorBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	virtual int32 GetLimit_Implementation() const override { return 1000; }

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual void Serialize(FArchive& Ar) override;

	//////////////////////////////////////////////////////////////////////////
	/// Ability
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "ActorStats")
	int32 Level;

public:
	virtual void OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAbilitySystemComponentBase* AbilitySystem;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAttributeSetBase* AttributeSet;

public:
	UFUNCTION(BlueprintPure)
	virtual int32 GetLevelV() const override { return Level; }

	UFUNCTION(BlueprintCallable)
	virtual bool SetLevelV(int32 InLevel) override;

	template<class T>
	T* GetAttributeSet() const
	{
		return Cast<T>(GetAttributeSet());
	}

	virtual UAttributeSetBase* GetAttributeSet() const override { return AttributeSet; }

	template<class T>
	T* GetAbilitySystemComponent() const
	{
		return Cast<T>(GetAbilitySystemComponent());
	}

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
};
