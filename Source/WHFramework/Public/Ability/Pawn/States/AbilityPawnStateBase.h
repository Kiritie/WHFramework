// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "FSM/Base/FiniteStateBase.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "AbilityPawnStateBase.generated.h"

/**
 *
 */
UCLASS()
class WHFRAMEWORK_API UAbilityPawnStateBase : public UFiniteStateBase
{
	GENERATED_BODY()

public:
	UAbilityPawnStateBase();

public:
	virtual void OnInitialize(UFSMComponent* InFSM, int32 InStateIndex) override;

	virtual bool OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams) override;

	virtual void OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams) override;

	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnLeave(UFiniteStateBase* InNextState) override;

	virtual void OnTermination() override;
	virtual void OnPresentationEnter(UFiniteStateBase* InLastState, const FFSMReplicatedState& State) override;

protected:
	void SetStateTag(FGameplayTag Tag, bool bPresent);
	void ClearStateTags();
	FGameplayTagContainer StateTags;
};
