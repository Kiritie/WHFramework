// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Event/Handle/EventHandleBase.h"
#include "Input/InputModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandle_AsyncLoadLevelFinished.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_AsyncLoadLevelFinished : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_AsyncLoadLevelFinished();

public:
	UPROPERTY(BlueprintReadOnly)
	FName LevelPath;
	
public:
	virtual void OnDespawn_Implementation() override;

	virtual void Fill_Implementation(const TArray<FParameter>& InParameters) override;
};
