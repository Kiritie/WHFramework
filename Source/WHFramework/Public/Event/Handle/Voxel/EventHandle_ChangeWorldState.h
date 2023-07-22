// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Event/Handle/EventHandleBase.h"
#include "Input/InputModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Voxel/VoxelModuleTypes.h"

#include "EventHandle_ChangeWorldState.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_ChangeWorldState : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_ChangeWorldState();

public:
	UPROPERTY(BlueprintReadOnly)
	EVoxelWorldState WorldState;
	
public:
	virtual void Fill_Implementation(const TArray<FParameter>& InParams) override;
};
