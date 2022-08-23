// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Auxiliary/VoxelAuxiliary.h"

#include "Ability/Components/VoxelInteractionComponent.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Chunks/VoxelChunk.h"

AVoxelAuxiliary::AVoxelAuxiliary()
{
	Interaction = CreateDefaultSubobject<UVoxelInteractionComponent>(FName("Interaction"));
	Interaction->SetupAttachment(RootComponent);
	Interaction->SetRelativeLocation(FVector(0, 0, 0));

	VoxelIndex = FIndex::ZeroIndex;
}

void AVoxelAuxiliary::BeginPlay()
{
	Super::BeginPlay();

}

void AVoxelAuxiliary::Initialize(FIndex InVoxelIndex)
{
	VoxelIndex = InVoxelIndex;

	const FVoxelItem& VoxelItem = GetVoxelItem();
	SetActorLocation(VoxelItem.GetLocation() + VoxelItem.GetRange() * UVoxelModuleBPLibrary::GetWorldData().BlockSize * 0.5f);
	Interaction->SetBoxExtent(VoxelItem.GetRange() * UVoxelModuleBPLibrary::GetWorldData().BlockSize * FVector(1.5f, 1.5f, 0.5f));
}

void AVoxelAuxiliary::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);
}

void AVoxelAuxiliary::OnDespawn_Implementation()
{
	Super::OnDespawn_Implementation();

	VoxelIndex = FIndex::ZeroIndex;
}

void AVoxelAuxiliary::OnEnterInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AVoxelAuxiliary::OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

bool AVoxelAuxiliary::CanInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
{
	return true;
}

void AVoxelAuxiliary::OnInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
{
	IInteractionAgentInterface::OnInteract(InInteractionAgent, InInteractAction);
}

FVoxelItem& AVoxelAuxiliary::GetVoxelItem() const
{
	if(Container)
	{
		return Cast<AVoxelChunk>(Container.GetObject())->GetVoxelItem(VoxelIndex);
	}
	return FVoxelItem::Empty;
}

UInteractionComponent* AVoxelAuxiliary::GetInteractionComponent() const
{
	return Interaction;
}
