// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityPawnStateBase.h"
#include "AbilityPawnState_Default.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAbilityPawnState_Default : public UAbilityPawnStateBase
{
	GENERATED_BODY()

	friend class AAbilityPawnBase;

public:
	UAbilityPawnState_Default();

public:
	virtual void OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex) override;

	virtual bool OnEnterValidate(UFiniteStateBase* InLastFiniteState) override;

	virtual void OnEnter(UFiniteStateBase* InLastFiniteState) override;
	
	virtual void OnRefresh() override;

	virtual void OnLeave(UFiniteStateBase* InNextFiniteState) override;

	virtual void OnTermination() override;
};
