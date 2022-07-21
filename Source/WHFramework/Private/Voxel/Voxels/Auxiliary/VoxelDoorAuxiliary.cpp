
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

// Called when the game starts or when spawned
void AVoxelDoorAuxiliary::BeginPlay()
{
	Super::BeginPlay();

}

void AVoxelDoorAuxiliary::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	OpenDoor();
}

void AVoxelDoorAuxiliary::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	CloseDoor();
}

void AVoxelDoorAuxiliary::Initialize(AVoxelChunk* InOwnerChunk, FIndex InVoxelIndex)
{
	Super::Initialize(InOwnerChunk, InVoxelIndex);
}

bool AVoxelDoorAuxiliary::CanInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
{
	if(!Super::CanInteract(InInteractionAgent, InInteractAction)) return false;
	
	switch (InInteractAction)
	{
		case EVoxelInteractAction::Open:
		{
			if(!GetVoxelItem().GetVoxel<UVoxelDoor>().IsOpened())
			{
				return true;
			}
			break;
		}
		case EVoxelInteractAction::Close:
		{
			if(GetVoxelItem().GetVoxel<UVoxelDoor>().IsOpened())
			{
				return true;
			}
			break;
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
	GetVoxelItem().GetVoxel<UVoxelDoor>().OpenTheDoor();
}

void AVoxelDoorAuxiliary::CloseDoor()
{
	GetVoxelItem().GetVoxel<UVoxelDoor>().CloseTheDoor();
}
