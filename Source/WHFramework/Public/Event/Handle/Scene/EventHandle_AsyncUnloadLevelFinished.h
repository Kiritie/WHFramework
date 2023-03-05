// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"
#include "Event/Handle/EventHandleBase.h"
#include "Input/InputModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandle_AsyncUnloadLevelFinished.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_AsyncUnloadLevelFinished : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_AsyncUnloadLevelFinished();

public:
	UPROPERTY(BlueprintReadOnly)
	FName LevelPath;
	
public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual void Fill_Implementation(const TArray<FParameter>& InParams) override;
};
