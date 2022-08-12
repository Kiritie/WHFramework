// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VoxelAuxiliary.h"
#include "VoxelDoorAuxiliary.generated.h"

class UVoxel;

/**
 */
UCLASS()
class WHFRAMEWORK_API AVoxelDoorAuxiliary : public AVoxelAuxiliary
{
	GENERATED_BODY()
	
public:
	AVoxelDoorAuxiliary();

public:
	virtual void Initialize(FIndex InVoxelIndex) override;

protected:
	virtual bool CanInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction) override;

	virtual void OnInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction) override;

	virtual void OpenDoor();

	virtual void CloseDoor();
};
