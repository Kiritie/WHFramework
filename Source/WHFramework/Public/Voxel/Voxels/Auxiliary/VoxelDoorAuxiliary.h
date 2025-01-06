// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VoxelInteractAuxiliary.h"
#include "VoxelDoorAuxiliary.generated.h"

class UVoxel;

/**
 */
UCLASS()
class WHFRAMEWORK_API AVoxelDoorAuxiliary : public AVoxelInteractAuxiliary
{
	GENERATED_BODY()
	
public:
	AVoxelDoorAuxiliary();

public:
	virtual bool CanInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent, bool bPassive) override;
};
