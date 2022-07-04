// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilityVitalityStateBase.h"
#include "UObject/Object.h"
#include "AbilityVitalityState_Default.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAbilityVitalityState_Default : public UAbilityVitalityStateBase
{
	GENERATED_BODY()

public:
	UAbilityVitalityState_Default();

public:
	virtual void OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex) override;

	virtual void OnEnter(UFiniteStateBase* InLastFiniteState) override;
	
	virtual void OnRefresh() override;

	virtual void OnLeave(UFiniteStateBase* InNextFiniteState) override;

	virtual void OnTermination() override;
};
