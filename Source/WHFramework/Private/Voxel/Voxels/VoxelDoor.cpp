// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/VoxelDoor.h"

#include "Voxel/VoxelModule.h"

UVoxelDoor::UVoxelDoor()
{
	
}

void UVoxelDoor::OnReset_Implementation()
{
	Super::OnReset_Implementation();
}

void UVoxelDoor::LoadData(const FString& InData)
{
	Super::LoadData(InData);
}

FString UVoxelDoor::ToData()
{
	return Super::ToData();
}

void UVoxelDoor::OnGenerate(IVoxelAgentInterface* InAgent)
{
	Super::OnGenerate(InAgent);
}

void UVoxelDoor::OnDestroy(IVoxelAgentInterface* InAgent)
{
	Super::OnDestroy(InAgent);
}

void UVoxelDoor::OnAgentHit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentHit(InAgent, InHitResult);
}

void UVoxelDoor::OnAgentEnter(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentEnter(InAgent, InHitResult);
}

void UVoxelDoor::OnAgentStay(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentStay(InAgent, InHitResult);
}

void UVoxelDoor::OnAgentExit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentExit(InAgent, InHitResult);
}

bool UVoxelDoor::OnAgentInteract(IVoxelAgentInterface* InAgent, EInputInteractAction InInteractAction, const FVoxelHitResult& InHitResult)
{
	return Super::OnAgentInteract(InAgent, InInteractAction, InHitResult);
}

void UVoxelDoor::Open()
{
	Super::Open();
}

void UVoxelDoor::Close()
{
	Super::Close();
}
