// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/VoxelWater.h"

#include "Components/SkeletalMeshComponent.h"
#include "Math/MathStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"

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
	
	if(!InAgent) return;
	
	if(GetOwner())
	{
		TMap<FIndex, FVoxelItem> VoxelItems;
		ITER_DIRECTION(Iter, 
			if(Iter != EDirection::Up && !GetOwner()->CheckVoxelAdjacent(Item, Iter))
			{
				VoxelItems.Emplace(GetIndex() + UMathStatics::DirectionToIndex(Iter), GetData().VoxelType);
			}
		)
		GetOwner()->SetVoxelComplex(VoxelItems, true, false, InAgent);
	}
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
