// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Event/Handle/EventHandleBase.h"
#include "Common/CommonTypes.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandle_GameUnPaused.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_GameUnPaused : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_GameUnPaused();
	
public:
	EPauseMode PauseMode;

public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void Parse_Implementation(const TArray<FParameter>& InParams) override;
};
