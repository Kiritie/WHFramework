// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "UObject/Object.h"
#include "Ability/Vitality/States/AbilityVitalityStateBase.h"
#include "AbilityVitalityState_Walk.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAbilityVitalityState_Walk : public UAbilityVitalityStateBase
{
	GENERATED_BODY()

	friend class AAbilityVitalityBase;

public:
	UAbilityVitalityState_Walk();

public:
	virtual void OnInitialize(UFSMComponent* InFSM, int32 InStateIndex) override;

	virtual bool OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams) override;

	virtual void OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams) override;
	
	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnLeave(UFiniteStateBase* InNextState) override;

	virtual void OnTermination() override;
};
