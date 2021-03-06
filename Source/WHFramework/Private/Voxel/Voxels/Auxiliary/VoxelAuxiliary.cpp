// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Auxiliary/VoxelAuxiliary.h"

#include "Ability/Components/VoxelInteractionComponent.h"
#include "Voxel/Chunks/VoxelChunk.h"

// Sets default values
AVoxelAuxiliary::AVoxelAuxiliary()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	Interaction = CreateDefaultSubobject<UVoxelInteractionComponent>(FName("Interaction"));
	Interaction->SetupAttachment(RootComponent);
	Interaction->SetRelativeLocation(FVector(0, 0, 0));

	VoxelIndex = FIndex::ZeroIndex;
}

// Called when the game starts or when spawned
void AVoxelAuxiliary::BeginPlay()
{
	Super::BeginPlay();

}

void AVoxelAuxiliary::Initialize(AVoxelChunk* InOwnerChunk, FIndex InVoxelIndex)
{
	VoxelIndex = InVoxelIndex;
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
	
}

FVoxelItem& AVoxelAuxiliary::GetVoxelItem() const
{
	if(Container)
	{
		return Cast<AVoxelChunk>(Container.GetObject())->GetVoxelItem(VoxelIndex);
	}
	return FVoxelItem::EmptyVoxel;
}

UInteractionComponent* AVoxelAuxiliary::GetInteractionComponent() const
{
	return Interaction;
}
