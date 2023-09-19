
// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Auxiliary/VoxelDoorAuxiliary.h"

#include "Ability/Components/VoxelInteractionComponent.h"
#include "Voxel/Voxels/VoxelDoor.h"

// Sets default values
AVoxelDoorAuxiliary::AVoxelDoorAuxiliary()
{
}

bool AVoxelDoorAuxiliary::CanInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent)
{
	return Super::CanInteract(InInteractAction, InInteractionAgent);
}

void AVoxelDoorAuxiliary::OnInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent, bool bPassivity)
{
	Super::OnInteract(InInteractAction, InInteractionAgent, bPassivity);
}