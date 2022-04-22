// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Event/Handle/EventHandleBase.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandle_EndProcedure.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_EndProcedure : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_EndProcedure();
	
public:
	virtual void OnDespawn_Implementation() override;

	virtual void Fill_Implementation(const TArray<FParameter>& InParameters) override;
};
