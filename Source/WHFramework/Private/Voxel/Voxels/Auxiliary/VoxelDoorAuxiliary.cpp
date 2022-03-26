
// Fill out your copyright notice in the Description page of Project Settings.


#include "VoxelAuxiliary/VoxelDoorAuxiliary.h"

#include "Voxel/Voxel.h"
#include "Character/Player/DWPlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Interaction/Components/VoxelInteractionComponent.h"
#include "Voxel/VoxelDoor.h"

// Sets default values
AVoxelDoorAuxiliary::AVoxelDoorAuxiliary()
{
	Interaction->AddInteractionAction(EInteractAction::Open);
	Interaction->AddInteractionAction(EInteractAction::Close);
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

bool AVoxelDoorAuxiliary::CanInteract(IInteraction* InInteractionTarget, EInteractAction InInteractAction)
{
	if(!Super::CanInteract(InInteractionTarget, InInteractAction)) return false;
	
	switch (InInteractAction)
	{
		case EInteractAction::Open:
		{
			if(!GetVoxelItem().GetParam(FName("bOpened")).GetBooleanValue())
			{
				return true;
			}
			break;
		}
		case EInteractAction::Close:
		{
			if(GetVoxelItem().GetParam(FName("bOpened")).GetBooleanValue())
			{
				return true;
			}
			break;
		}
		default: return true;
	}
	return false;
}

void AVoxelDoorAuxiliary::OnInteract(IInteraction* InTrigger, EInteractAction InInteractAction)
{
	Super::OnInteract(InTrigger, InInteractAction);

	switch (InInteractAction)
	{
		case EInteractAction::Open:
		{
			OpenDoor();
			break;
		}
		case EInteractAction::Close:
		{
			CloseDoor();
			break;
		}
		default: break;
	}
}

void AVoxelDoorAuxiliary::OpenDoor()
{
	if(UVoxel* Voxel = GetVoxelItem().GetVoxel())
	{
		if(UVoxelDoor* VoxelDoor = Cast<UVoxelDoor>(Voxel))
		{
			VoxelDoor->OpenTheDoor();
		}
		UVoxel::DespawnVoxel(Voxel);
	}
}

void AVoxelDoorAuxiliary::CloseDoor()
{
	if(UVoxel* Voxel = GetVoxelItem().GetVoxel())
	{
		if(UVoxelDoor* VoxelDoor = Cast<UVoxelDoor>(Voxel))
		{
			VoxelDoor->CloseTheDoor();
		}
		UVoxel::DespawnVoxel(Voxel);
	}
}
