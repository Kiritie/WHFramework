// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Event/Handle/EventHandleBase.h"
#include "Global/GlobalTypes.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandle_UnPauseGame.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_UnPauseGame : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_UnPauseGame();
	
public:
	EPauseMode PauseMode;

public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void Fill_Implementation(const TArray<FParameter>& InParams) override;
};
