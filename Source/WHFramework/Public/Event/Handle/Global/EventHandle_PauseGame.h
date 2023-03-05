// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"
#include "Event/Handle/EventHandleBase.h"
#include "Global/GlobalTypes.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandle_PauseGame.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_PauseGame : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_PauseGame();
	
public:
	EPauseGameMode PauseGameMode;

public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void Fill_Implementation(const TArray<FParameter>& InParams) override;
};
