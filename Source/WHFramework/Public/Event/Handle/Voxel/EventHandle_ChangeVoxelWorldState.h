// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Event/Handle/EventHandleBase.h"
#include "Input/InputModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Voxel/VoxelModuleTypes.h"

#include "EventHandle_ChangeVoxelWorldState.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_ChangeVoxelWorldState : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_ChangeVoxelWorldState();

public:
	UPROPERTY(BlueprintReadOnly)
	EVoxelWorldState WorldState;
	
public:
	virtual void Fill_Implementation(const TArray<FParameter>& InParams) override;
};
