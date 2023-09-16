// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/VoxelDoor.h"

#include "Audio/AudioModuleBPLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Datas/VoxelDoorData.h"

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

bool UVoxelDoor::OnAgentInteract(IVoxelAgentInterface* InAgent, EVoxelInteractType InActionType, const FVoxelHitResult& InHitResult)
{
	return Super::OnAgentInteract(InAgent, InActionType, InHitResult);
}

void UVoxelDoor::Open()
{
	Super::Open();
}

void UVoxelDoor::Close()
{
	Super::Close();
}
