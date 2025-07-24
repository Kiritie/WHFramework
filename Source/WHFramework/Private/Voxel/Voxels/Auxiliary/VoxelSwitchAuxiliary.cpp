
// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Auxiliary/VoxelSwitchAuxiliary.h"

#include "Common/Interaction/InteractionComponent.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Agent/VoxelAgentInterface.h"
#include "Voxel/Voxels/VoxelSwitch.h"

// Sets default values
AVoxelSwitchAuxiliary::AVoxelSwitchAuxiliary()
{
	
}

bool AVoxelSwitchAuxiliary::CanInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent)
{
	switch ((EVoxelInteractAction)InInteractAction)
	{
		case EVoxelInteractAction::Open:
		{
			if(Cast<IVoxelAgentInterface>(InInteractionAgent))
			{
				return !GetVoxelItem(true).GetVoxel<UVoxelSwitch>().IsOpened();
			}
		}
		case EVoxelInteractAction::Close:
		{
			if(Cast<IVoxelAgentInterface>(InInteractionAgent))
			{
				return GetVoxelItem(true).GetVoxel<UVoxelSwitch>().IsOpened();
			}
		}
		default: break;
	}
	return false;
}

void AVoxelSwitchAuxiliary::OnEnterInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AVoxelSwitchAuxiliary::OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AVoxelSwitchAuxiliary::OnInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent, bool bPassive)
{
	if(bPassive)
	{
		switch ((EVoxelInteractAction)InInteractAction)
		{
			case EVoxelInteractAction::Open:
			{
				GetVoxelItem().GetVoxel<UVoxelSwitch>().Open(nullptr);
				break;
			}
			case EVoxelInteractAction::Close:
			{
				GetVoxelItem().GetVoxel<UVoxelSwitch>().Close(nullptr);
				break;
			}
			default: break;
		}
	}
}
