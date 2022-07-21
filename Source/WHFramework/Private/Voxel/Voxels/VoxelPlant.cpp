// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/VoxelPlant.h"

UVoxelPlant::UVoxelPlant()
{
	
}

void UVoxelPlant::LoadData(FSaveData* InSaveData, bool bForceMode)
{
	Super::LoadData(InSaveData, bForceMode);
}

FSaveData* UVoxelPlant::ToData()
{
	return Super::ToData();
}

void UVoxelPlant::OnTargetHit(IVoxelAgentInterface* InTarget, const FVoxelHitResult& InHitResult)
{
	Super::OnTargetHit(InTarget, InHitResult);
}

void UVoxelPlant::OnTargetEnter(IVoxelAgentInterface* InTarget, const FVoxelHitResult& InHitResult)
{
	Super::OnTargetEnter(InTarget, InHitResult);
}

void UVoxelPlant::OnTargetStay(IVoxelAgentInterface* InTarget, const FVoxelHitResult& InHitResult)
{
	Super::OnTargetStay(InTarget, InHitResult);
}

void UVoxelPlant::OnTargetExit(IVoxelAgentInterface* InTarget, const FVoxelHitResult& InHitResult)
{
	Super::OnTargetExit(InTarget, InHitResult);
}

bool UVoxelPlant::OnMouseDown(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult)
{
	switch (InMouseButton)
	{
		case EMouseButton::Left:
		{
			return Super::OnMouseDown(InMouseButton, InHitResult);
		}
		default: break;
	}
	return false;
}

bool UVoxelPlant::OnMouseUp(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult)
{
	return Super::OnMouseUp(InMouseButton, InHitResult);
}

bool UVoxelPlant::OnMouseHold(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult)
{
	return Super::OnMouseHold(InMouseButton, InHitResult);
}

void UVoxelPlant::OnMouseHover(const FVoxelHitResult& InHitResult)
{
	Super::OnMouseHover(InHitResult);
}
