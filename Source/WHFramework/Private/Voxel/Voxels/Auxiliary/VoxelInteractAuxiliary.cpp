
// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Auxiliary/VoxelInteractAuxiliary.h"

#include "Ability/Components/VoxelInteractionComponent.h"
#include "Voxel/Voxels/VoxelInteract.h"

// Sets default values
AVoxelInteractAuxiliary::AVoxelInteractAuxiliary()
{
	Interaction->AddInteractionAction((EInteractAction)EVoxelInteractAction::Open);
	Interaction->AddInteractionAction((EInteractAction)EVoxelInteractAction::Close);
}

bool AVoxelInteractAuxiliary::CanInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
{
	if(!Super::CanInteract(InInteractionAgent, InInteractAction)) return false;
	
	switch (InInteractAction)
	{
		case EVoxelInteractAction::Open:
		{
			return !GetVoxelItem().GetVoxel<UVoxelInteract>().IsOpened();
		}
		case EVoxelInteractAction::Close:
		{
			return GetVoxelItem().GetVoxel<UVoxelInteract>().IsOpened();
		}
		default: return true;
	}
}

void AVoxelInteractAuxiliary::OnInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
{
	Super::OnInteract(InInteractionAgent, InInteractAction);

	switch (InInteractAction)
	{
		case EVoxelInteractAction::Open:
		{
			Open();
			break;
		}
		case EVoxelInteractAction::Close:
		{
			Close();
			break;
		}
		default: break;
	}
}

void AVoxelInteractAuxiliary::Open()
{
	GetVoxelItem().GetVoxel<UVoxelInteract>().Open();
}

void AVoxelInteractAuxiliary::Close()
{
	GetVoxelItem().GetVoxel<UVoxelInteract>().Close();
}
