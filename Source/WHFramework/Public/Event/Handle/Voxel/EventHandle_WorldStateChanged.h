// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Event/Handle/EventHandleBase.h"
#include "Input/InputModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Voxel/VoxelModuleTypes.h"

#include "EventHandle_WorldStateChanged.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_WorldStateChanged : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_WorldStateChanged();
	
public:
	virtual void Parse_Implementation(const TArray<FParameter>& InParams) override;

public:
	UPROPERTY(BlueprintReadOnly)
	EVoxelWorldState WorldState;
};
