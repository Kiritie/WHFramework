// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/VoxelWater.h"

UVoxelWater::UVoxelWater()
{
	
}

void UVoxelWater::LoadData(const FString& InData)
{
	Super::LoadData(InData);
}

FString UVoxelWater::ToData()
{
	return Super::ToData();
}

void UVoxelWater::OnGenerate(IVoxelAgentInterface* InAgent)
{
	Super::OnGenerate(InAgent);
}

void UVoxelWater::OnDestroy(IVoxelAgentInterface* InAgent)
{
	Super::OnDestroy(InAgent);
}

void UVoxelWater::OnAgentHit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentHit(InAgent, InHitResult);
}

void UVoxelWater::OnAgentEnter(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentEnter(InAgent, InHitResult);
}

void UVoxelWater::OnAgentStay(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentStay(InAgent, InHitResult);
}

void UVoxelWater::OnAgentExit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentExit(InAgent, InHitResult);
}

bool UVoxelWater::OnAgentInteract(IVoxelAgentInterface* InAgent, EInputInteractAction InInteractAction, EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult)
{
	return false;
}
