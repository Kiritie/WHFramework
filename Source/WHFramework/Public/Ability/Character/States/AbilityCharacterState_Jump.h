// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "UObject/Object.h"
#include "Ability/Character/States/AbilityCharacterStateBase.h"
#include "AbilityCharacterState_Jump.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAbilityCharacterState_Jump : public UAbilityCharacterStateBase
{
	GENERATED_BODY()

	friend class AAbilityCharacterBase;

public:
	UAbilityCharacterState_Jump();

public:
	virtual void OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex) override;

	virtual bool OnEnterValidate(UFiniteStateBase* InLastFiniteState) override;

	virtual void OnEnter(UFiniteStateBase* InLastFiniteState) override;
	
	virtual void OnRefresh() override;

	virtual void OnLeave(UFiniteStateBase* InNextFiniteState) override;

	virtual void OnTermination() override;
};
