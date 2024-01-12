// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Event/Handle/EventHandleBase.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandle_GlobalParameterChanged.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_GlobalParameterChanged : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_GlobalParameterChanged();

public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual void Parse_Implementation(const TArray<FParameter>& InParams) override;

public:
	UPROPERTY(BlueprintReadOnly)
	FName ParameterName;
	
	UPROPERTY(BlueprintReadOnly)
	FParameter Parameter;
};
