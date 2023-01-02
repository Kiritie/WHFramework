// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"
#include "AbilityCharacterStateBase.h"
#include "UObject/Object.h"
#include "AbilityCharacterState_Default.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAbilityCharacterState_Default : public UAbilityCharacterStateBase
{
	GENERATED_BODY()

	friend class AAbilityCharacterBase;

public:
	UAbilityCharacterState_Default();

public:
	virtual void OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex) override;

	virtual bool OnEnterValidate(UFiniteStateBase* InLastFiniteState) override;

	virtual void OnEnter(UFiniteStateBase* InLastFiniteState) override;
	
	virtual void OnRefresh() override;

	virtual void OnLeave(UFiniteStateBase* InNextFiniteState) override;

	virtual void OnTermination() override;
};
