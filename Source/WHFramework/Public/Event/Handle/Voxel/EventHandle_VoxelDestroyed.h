// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Event/Handle/EventHandleBase.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Voxel/VoxelModuleTypes.h"

#include "EventHandle_VoxelDestroyed.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_VoxelDestroyed : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_VoxelDestroyed();
	
public:
	virtual void Parse_Implementation(const TArray<FParameter>& InParams) override;

public:
	UPROPERTY(BlueprintReadOnly)
	FVoxelItem VoxelItem;
};
