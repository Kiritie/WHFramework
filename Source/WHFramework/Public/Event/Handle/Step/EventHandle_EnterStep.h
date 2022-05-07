// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Event/Handle/EventHandleBase.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandle_EnterStep.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_EnterStep : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_EnterStep();

public:
	UPROPERTY(BlueprintReadOnly)
	class UStepBase* Step;
	
public:
	virtual void OnDespawn_Implementation() override;

	virtual void Fill_Implementation(const TArray<FParameter>& InParams) override;
};
