// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Event/Handle/EventHandleBase.h"
#include "Input/InputModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Voxel/VoxelModuleTypes.h"

#include "EventHandle_VoxelGenerated.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_VoxelGenerated : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_VoxelGenerated();
	
public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;
	
	virtual void Parse_Implementation(const TArray<FParameter>& InParams) override;

public:
	UPROPERTY(BlueprintReadOnly)
	FVoxelItem VoxelItem;

	UPROPERTY(BlueprintReadOnly)
	TScriptInterface<IVoxelAgentInterface> VoxelAgent;
};
