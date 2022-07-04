// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FSM/Base/FiniteStateBase.h"
#include "UObject/Object.h"
#include "AbilityVitalityStateBase.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAbilityVitalityStateBase : public UFiniteStateBase
{
	GENERATED_BODY()

public:
	UAbilityVitalityStateBase();

public:
	virtual void OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex) override;

	virtual void OnEnter(UFiniteStateBase* InLastFiniteState) override;
	
	virtual void OnRefresh() override;

	virtual void OnLeave(UFiniteStateBase* InNextFiniteState) override;

	virtual void OnTermination() override;
};
