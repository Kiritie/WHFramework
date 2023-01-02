// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"
#include "Event/Handle/EventHandleBase.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandle_EndStep.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_EndStep : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_EndStep();
	
public:
	virtual void OnDespawn_Implementation() override;

public:
	virtual void Fill_Implementation(const TArray<FParameter>& InParams) override;
};
