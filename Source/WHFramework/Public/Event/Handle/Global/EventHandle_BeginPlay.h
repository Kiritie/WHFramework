// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Event/Handle/EventHandleBase.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandle_BeginPlay.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_BeginPlay : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_BeginPlay();
	
public:
	bool bIsSimulating;

public:
	virtual void OnDespawn_Implementation() override;

public:
	virtual void Fill_Implementation(const TArray<FParameter>& InParams) override;
};
