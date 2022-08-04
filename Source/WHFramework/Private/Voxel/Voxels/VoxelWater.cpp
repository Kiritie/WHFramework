// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/VoxelWater.h"

#include "GameFramework/PawnMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"

UVoxelWater::UVoxelWater()
{
	
}

void UVoxelWater::LoadData(FSaveData* InSaveData, bool bForceMode)
{
	Super::LoadData(InSaveData, bForceMode);
}

FSaveData* UVoxelWater::ToData()
{
	return Super::ToData();
}

void UVoxelWater::OnGenerate(IVoxelAgentInterface* InAgent)
{
	Super::OnGenerate(InAgent);
}

void UVoxelWater::OnReplace(IVoxelAgentInterface* InAgent, const FVoxelItem& InOldVoxelItem)
{
	Super::OnReplace(InAgent, InOldVoxelItem);
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

bool UVoxelWater::OnActionTrigger(IVoxelAgentInterface* InAgent, EVoxelActionType InActionType, const FVoxelHitResult& InHitResult)
{
	return false;
}
