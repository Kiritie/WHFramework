// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Audio/AudioModuleTypes.h"
#include "Event/Handle/EventHandleBase.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandle_SoundParamsChanged.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_SoundParamsChanged : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_SoundParamsChanged();

public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual void Parse_Implementation(const TArray<FParameter>& InParams) override;

public:
	UPROPERTY(BlueprintReadOnly)
	USoundClass* SoundClass;
	
	UPROPERTY(BlueprintReadOnly)
	FSoundParams SoundParams;
};
