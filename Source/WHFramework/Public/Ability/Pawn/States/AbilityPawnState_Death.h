// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityPawnStateBase.h"
#include "AbilityPawnState_Death.generated.h"

class IAbilityVitalityInterface;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAbilityPawnState_Death : public UAbilityPawnStateBase
{
	GENERATED_BODY()

	friend class AAbilityPawnBase;

public:
	UAbilityPawnState_Death();

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
};
