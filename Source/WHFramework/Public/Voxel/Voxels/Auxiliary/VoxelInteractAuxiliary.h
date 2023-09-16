// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VoxelAuxiliary.h"
#include "VoxelInteractAuxiliary.generated.h"

class UVoxel;

/**
 */
UCLASS()
class WHFRAMEWORK_API AVoxelInteractAuxiliary : public AVoxelAuxiliary
{
	GENERATED_BODY()
	
public:
	AVoxelInteractAuxiliary();

public:
	virtual bool CanInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction) override;

	virtual void OnInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction) override;

public:
	UFUNCTION(BlueprintCallable)
	virtual void Open();

	UFUNCTION(BlueprintCallable)
	virtual void Close();
};
