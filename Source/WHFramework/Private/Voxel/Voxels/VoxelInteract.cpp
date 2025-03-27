// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/VoxelInteract.h"

#include "Voxel/Agent/VoxelAgentInterface.h"

UVoxelInteract::UVoxelInteract()
{
}

void UVoxelInteract::OnReset_Implementation()
{
	Super::OnReset_Implementation();
}

void UVoxelInteract::LoadData(const FString& InData)
{
	Super::LoadData(InData);
}

FString UVoxelInteract::ToData()
{
	return Super::ToData();
}

void UVoxelInteract::OnGenerate(IVoxelAgentInterface* InAgent)
{
	Super::OnGenerate(InAgent);
}

void UVoxelInteract::OnDestroy(IVoxelAgentInterface* InAgent)
{
	Super::OnDestroy(InAgent);
}

void UVoxelInteract::OnAgentHit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentHit(InAgent, InHitResult);
}

void UVoxelInteract::OnAgentEnter(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentEnter(InAgent, InHitResult);
}

void UVoxelInteract::OnAgentStay(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentStay(InAgent, InHitResult);
}

void UVoxelInteract::OnAgentExit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentExit(InAgent, InHitResult);
}

bool UVoxelInteract::OnAgentInteract(IVoxelAgentInterface* InAgent, EInputInteractAction InInteractAction, EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult)
{
	return Super::OnAgentInteract(InAgent, InInteractAction, InInteractEvent, InHitResult);
}

bool UVoxelInteract::Interact(IVoxelAgentInterface* InAgent)
{
	return false;
}

void UVoxelInteract::UnInteract(IVoxelAgentInterface* InAgent)
{
	
}
