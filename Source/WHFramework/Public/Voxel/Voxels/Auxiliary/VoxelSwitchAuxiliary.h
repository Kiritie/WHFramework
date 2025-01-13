// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VoxelInteractAuxiliary.h"
#include "VoxelSwitchAuxiliary.generated.h"

class UBoxComponent;
class UVoxel;
class UInteractionComponent;

/**
 */
UCLASS()
class WHFRAMEWORK_API AVoxelSwitchAuxiliary : public AVoxelInteractAuxiliary
{
	GENERATED_BODY()
	
public:
	AVoxelSwitchAuxiliary();
	
public:
	virtual bool CanInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnEnterInteract(IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent, bool bPassive) override;
};
