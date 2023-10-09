// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/VoxelContainer.h"

#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Voxels/Auxiliary/VoxelContainerAuxiliary.h"

UVoxelContainer::UVoxelContainer()
{
	
}

void UVoxelContainer::OnReset_Implementation()
{
	Super::OnReset_Implementation();
}

void UVoxelContainer::LoadData(const FString& InData)
{
	Super::LoadData(InData);
}

FString UVoxelContainer::ToData()
{
	return Super::ToData();
}

void UVoxelContainer::OnGenerate(IVoxelAgentInterface* InAgent)
{
	Super::OnGenerate(InAgent);
}

void UVoxelContainer::OnDestroy(IVoxelAgentInterface* InAgent)
{
	Super::OnDestroy(InAgent);

	if(GetAuxiliary<AVoxelContainerAuxiliary>())
	{
		GetAuxiliary<AVoxelContainerAuxiliary>()->GetInventory()->DiscardAllItem();
	}
}

void UVoxelContainer::OnAgentHit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentHit(InAgent, InHitResult);
}

void UVoxelContainer::OnAgentEnter(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentEnter(InAgent, InHitResult);
}

void UVoxelContainer::OnAgentStay(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentStay(InAgent, InHitResult);
}

void UVoxelContainer::OnAgentExit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentExit(InAgent, InHitResult);
}

bool UVoxelContainer::OnAgentInteract(IVoxelAgentInterface* InAgent, EInputInteractAction InActionType, const FVoxelHitResult& InHitResult)
{
	return Super::OnAgentInteract(InAgent, InActionType, InHitResult);
}

void UVoxelContainer::Open(IVoxelAgentInterface* InAgent)
{
	Super::Open(InAgent);
}

void UVoxelContainer::Close(IVoxelAgentInterface* InAgent)
{
	Super::Close(InAgent);
}
