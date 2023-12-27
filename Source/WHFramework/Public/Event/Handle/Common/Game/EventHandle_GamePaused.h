// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Event/Handle/EventHandleBase.h"
#include "Common/CommonTypes.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandle_GamePaused.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_GamePaused : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_GamePaused();

public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void Parse_Implementation(const TArray<FParameter>& InParams) override;
	
public:
	UPROPERTY(BlueprintReadOnly)
	EPauseMode PauseMode;
};
