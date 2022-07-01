// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Event/Handle/EventHandleBase.h"
#include "EventHandle_LeaveFiniteState.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_LeaveFiniteState : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_LeaveFiniteState();

public:
	UPROPERTY(BlueprintReadOnly)
	class UFiniteStateBase* State;
	UPROPERTY(BlueprintReadOnly)
	class UFSMComponent* FSM;

public:
	virtual void OnDespawn_Implementation() override;

	virtual void Fill_Implementation(const TArray<FParameter>& InParams) override;
};
