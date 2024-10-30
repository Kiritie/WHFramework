// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityPawnStateBase.h"
#include "AbilityPawnState_Spawn.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAbilityPawnState_Spawn : public UAbilityPawnStateBase
{
	GENERATED_BODY()

	friend class AAbilityPawnBase;

public:
	UAbilityPawnState_Spawn();

public:
	virtual void OnInitialize(UFSMComponent* InFSM, int32 InStateIndex) override;

	virtual bool OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams) override;

	virtual void OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams) override;
	
	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnLeave(UFiniteStateBase* InNextState) override;

	virtual void OnTermination() override;

protected:
	virtual void TryLeave();
};
