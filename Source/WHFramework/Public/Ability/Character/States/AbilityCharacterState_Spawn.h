// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityCharacterStateBase.h"
#include "UObject/Object.h"
#include "AbilityCharacterState_Spawn.generated.h"

class IAbilityVitalityInterface;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAbilityCharacterState_Spawn : public UAbilityCharacterStateBase
{
	GENERATED_BODY()

	friend class AAbilityCharacterBase;

public:
	UAbilityCharacterState_Spawn();

public:
	virtual void OnInitialize(UFSMComponent* InFSM, int32 InStateIndex) override;

	virtual bool OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams) override;

	virtual void OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams) override;
	
	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnLeave(UFiniteStateBase* InNextState) override;

	virtual void OnTermination() override;

protected:
	virtual void TryLeave();

protected:
	IAbilityVitalityInterface* Rescuer;
};
