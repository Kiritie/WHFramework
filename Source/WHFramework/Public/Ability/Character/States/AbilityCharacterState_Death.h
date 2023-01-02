// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"
#include "AbilityCharacterStateBase.h"
#include "UObject/Object.h"
#include "AbilityCharacterState_Death.generated.h"

class IAbilityVitalityInterface;
class AAbilityCharacterBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAbilityCharacterState_Death : public UAbilityCharacterStateBase
{
	GENERATED_BODY()

	friend class AAbilityCharacterBase;

public:
	UAbilityCharacterState_Death();

public:
	virtual void OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex) override;

	virtual bool OnEnterValidate(UFiniteStateBase* InLastFiniteState) override;

	virtual void OnEnter(UFiniteStateBase* InLastFiniteState) override;
	
	virtual void OnRefresh() override;

	virtual void OnLeave(UFiniteStateBase* InNextFiniteState) override;

	virtual void OnTermination() override;

public:
	UFUNCTION(BlueprintCallable)
	virtual void DeathStart();

	UFUNCTION(BlueprintCallable)
	virtual void DeathEnd();

protected:
	IAbilityVitalityInterface* Killer;

	bool bDeathStarted;
};
