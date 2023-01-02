// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"
#include "UObject/Object.h"
#include "Ability/Character/States/AbilityCharacterStateBase.h"
#include "AbilityCharacterState_Walk.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAbilityCharacterState_Walk : public UAbilityCharacterStateBase
{
	GENERATED_BODY()

	friend class AAbilityCharacterBase;

public:
	UAbilityCharacterState_Walk();

public:
	virtual void OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex) override;

	virtual bool OnEnterValidate(UFiniteStateBase* InLastFiniteState) override;

	virtual void OnEnter(UFiniteStateBase* InLastFiniteState) override;
	
	virtual void OnRefresh() override;

	virtual bool OnLeaveValidate(UFiniteStateBase* InNextFiniteState) override;

	virtual void OnLeave(UFiniteStateBase* InNextFiniteState) override;

	virtual void OnTermination() override;
};
