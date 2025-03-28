// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/VoxelTorch.h"

#include "Voxel/Voxels/Auxiliary/VoxelTorchAuxiliary.h"

UVoxelTorch::UVoxelTorch()
{
	bOpened = true;
}

void UVoxelTorch::OnReset_Implementation()
{
	Super::OnReset_Implementation();

	bOpened = true;
}

void UVoxelTorch::LoadData(const FString& InData)
{
	Super::LoadData(InData);
}

FString UVoxelTorch::ToData()
{
	return Super::ToData();
}

void UVoxelTorch::OnGenerate(IVoxelAgentInterface* InAgent)
{
	Super::OnGenerate(InAgent);

	if(GetAuxiliary<AVoxelTorchAuxiliary>())
	{
		GetAuxiliary<AVoxelTorchAuxiliary>()->SetLightVisible(bOpened);
	}
}

void UVoxelTorch::OnDestroy(IVoxelAgentInterface* InAgent)
{
	Super::OnDestroy(InAgent);
}

void UVoxelTorch::OnAgentHit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentHit(InAgent, InHitResult);
}

void UVoxelTorch::OnAgentEnter(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentEnter(InAgent, InHitResult);
}

void UVoxelTorch::OnAgentStay(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentStay(InAgent, InHitResult);
}

void UVoxelTorch::OnAgentExit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentExit(InAgent, InHitResult);
}

bool UVoxelTorch::OnAgentInteract(IVoxelAgentInterface* InAgent, EInputInteractAction InInteractAction, EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult)
{
	return Super::OnAgentInteract(InAgent, InInteractAction, InInteractEvent, InHitResult);
}

void UVoxelTorch::Open(IVoxelAgentInterface* InAgent)
{
	Super::Open(InAgent);
	
	if(GetAuxiliary<AVoxelTorchAuxiliary>())
	{
		GetAuxiliary<AVoxelTorchAuxiliary>()->SetLightVisible(true);
	}
}

void UVoxelTorch::Close(IVoxelAgentInterface* InAgent)
{
	Super::Close(InAgent);

	if(GetAuxiliary<AVoxelTorchAuxiliary>())
	{
		GetAuxiliary<AVoxelTorchAuxiliary>()->SetLightVisible(false);
	}
}
