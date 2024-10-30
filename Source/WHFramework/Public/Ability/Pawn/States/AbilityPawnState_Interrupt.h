﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Ability/Pawn/States/AbilityPawnStateBase.h"
#include "AbilityPawnState_Interrupt.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAbilityPawnState_Interrupt : public UAbilityPawnStateBase
{
	GENERATED_BODY()

	friend class AAbilityPawnBase;

public:
	UAbilityPawnState_Interrupt();

public:
	virtual void OnInitialize(UFSMComponent* InFSM, int32 InStateIndex) override;

	virtual bool OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams) override;

	virtual void OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams) override;
	
	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnLeave(UFiniteStateBase* InNextState) override;

	virtual void OnTermination() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Duration;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float RemainTime;
};
