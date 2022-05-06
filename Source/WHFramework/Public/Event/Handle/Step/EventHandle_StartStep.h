// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Event/Handle/EventHandleBase.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandle_StartStep.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_StartStep : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_StartStep();

public:
	UPROPERTY(BlueprintReadOnly)
	int32 RootStepIndex;
	
public:
	virtual void OnDespawn_Implementation() override;

	virtual void Fill_Implementation(const TArray<FParameter>& InParameters) override;
};
