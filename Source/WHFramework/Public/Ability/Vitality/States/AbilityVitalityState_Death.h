// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilityVitalityStateBase.h"
#include "UObject/Object.h"
#include "AbilityVitalityState_Death.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAbilityVitalityState_Death : public UAbilityVitalityStateBase
{
	GENERATED_BODY()

public:
	UAbilityVitalityState_Death();

public:
	virtual void OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex) override;

	virtual void OnEnter(UFiniteStateBase* InLastFiniteState) override;
	
	virtual void OnRefresh() override;

	virtual void OnLeave(UFiniteStateBase* InNextFiniteState) override;

	virtual void OnTermination() override;

public:
	UFUNCTION(BlueprintCallable)
	virtual void DeathStart() override;

	UFUNCTION(BlueprintCallable)
	virtual void DeathEnd() override;
};
