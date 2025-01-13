// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/VoxelBed.h"

UVoxelBed::UVoxelBed()
{
	
}

void UVoxelBed::LoadData(const FString& InData)
{
	UVoxel::LoadData(InData);
}

FString UVoxelBed::ToData()
{
	return UVoxel::ToData();
}

void UVoxelBed::OnGenerate(IVoxelAgentInterface* InAgent)
{
	Super::OnGenerate(InAgent);
}

void UVoxelBed::OnDestroy(IVoxelAgentInterface* InAgent)
{
	Super::OnDestroy(InAgent);
}

void UVoxelBed::OnAgentHit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentHit(InAgent, InHitResult);
}

void UVoxelBed::OnAgentEnter(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentEnter(InAgent, InHitResult);
}

void UVoxelBed::OnAgentStay(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentStay(InAgent, InHitResult);
}

void UVoxelBed::OnAgentExit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentExit(InAgent, InHitResult);
}

bool UVoxelBed::OnAgentInteract(IVoxelAgentInterface* InAgent, EInputInteractAction InInteractAction, EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult)
{
	return Super::OnAgentInteract(InAgent, InInteractAction, InInteractEvent, InHitResult);
}

void UVoxelBed::Interact(IVoxelAgentInterface* InAgent)
{
	// TODO: Sleep
}
