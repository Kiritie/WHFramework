
// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Auxiliary/VoxelDoorAuxiliary.h"

#include "Ability/Components/VoxelInteractionComponent.h"
#include "Voxel/Voxels/VoxelDoor.h"

// Sets default values
AVoxelDoorAuxiliary::AVoxelDoorAuxiliary()
{
}

bool AVoxelDoorAuxiliary::CanInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
{
	return Super::CanInteract(InInteractionAgent, InInteractAction);
}

void AVoxelDoorAuxiliary::OnInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
{
	Super::OnInteract(InInteractionAgent, InInteractAction);
}