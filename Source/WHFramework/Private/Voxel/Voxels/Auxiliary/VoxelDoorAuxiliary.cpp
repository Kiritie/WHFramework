
// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Auxiliary/VoxelDoorAuxiliary.h"

#include "Ability/Components/VoxelInteractionComponent.h"
#include "Voxel/Voxels/VoxelDoor.h"

// Sets default values
AVoxelDoorAuxiliary::AVoxelDoorAuxiliary()
{
	Interaction->AddInteractionAction((EInteractAction)EVoxelInteractAction::Open);
	Interaction->AddInteractionAction((EInteractAction)EVoxelInteractAction::Close);
}

void AVoxelDoorAuxiliary::Initialize(FIndex InVoxelIndex)
{
	Super::Initialize(InVoxelIndex);
}

bool AVoxelDoorAuxiliary::CanInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
{
	if(!Super::CanInteract(InInteractionAgent, InInteractAction)) return false;
	
	switch (InInteractAction)
	{
		case EVoxelInteractAction::Open:
		{
			return !GetVoxelItem().GetVoxel<UVoxelDoor>().IsOpened();
		}
		case EVoxelInteractAction::Close:
		{
			return GetVoxelItem().GetVoxel<UVoxelDoor>().IsOpened();
		}
		default: return true;
	}
	return false;
}

void AVoxelDoorAuxiliary::OnInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
{
	Super::OnInteract(InInteractionAgent, InInteractAction);

	switch (InInteractAction)
	{
		case EVoxelInteractAction::Open:
		{
			OpenDoor();
			break;
		}
		case EVoxelInteractAction::Close:
		{
			CloseDoor();
			break;
		}
		default: break;
	}
}

void AVoxelDoorAuxiliary::OpenDoor()
{
	GetVoxelItem().GetVoxel<UVoxelDoor>().Open();
}

void AVoxelDoorAuxiliary::CloseDoor()
{
	GetVoxelItem().GetVoxel<UVoxelDoor>().Close();
}
