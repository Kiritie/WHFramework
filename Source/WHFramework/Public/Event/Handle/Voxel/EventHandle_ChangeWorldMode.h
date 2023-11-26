// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Event/Handle/EventHandleBase.h"
#include "Input/InputModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Voxel/VoxelModuleTypes.h"

#include "EventHandle_ChangeWorldMode.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_ChangeWorldMode : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_ChangeWorldMode();
	
public:
	virtual void Fill_Implementation(const TArray<FParameter>& InParams) override;

public:
	UPROPERTY(BlueprintReadOnly)
	EVoxelWorldMode WorldMode;
};
