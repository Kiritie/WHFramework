// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/VoxelTorch.h"

#include "Components/PointLightComponent.h"
#include "Voxel/Voxels/Auxiliary/VoxelTorchAuxiliary.h"

UVoxelTorch::UVoxelTorch()
{
	bOn = true;
}

void UVoxelTorch::OnReset_Implementation()
{
	Super::OnReset_Implementation();

	bOn = true;
}

void UVoxelTorch::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(Ar.ArIsSaveGame)
	{
		if(Ar.IsLoading())
		{
			Ar << bOn;
		}
		else if(Ar.IsSaving())
		{
			Ar << bOn;
		}
	}
}

void UVoxelTorch::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);
}

FSaveData* UVoxelTorch::ToData(bool bRefresh)
{
	return Super::ToData(bRefresh);
}

void UVoxelTorch::OnGenerate(IVoxelAgentInterface* InAgent)
{
	Super::OnGenerate(InAgent);

	if(GetAuxiliary<AVoxelTorchAuxiliary>())
	{
		GetAuxiliary<AVoxelTorchAuxiliary>()->SetLightVisible(bOn);
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

bool UVoxelTorch::OnActionTrigger(IVoxelAgentInterface* InAgent, EVoxelActionType InActionType, const FVoxelHitResult& InHitResult)
{
	switch (InActionType)
	{
		case EVoxelActionType::Action1:
		{
			return Super::OnActionTrigger(InAgent, InActionType, InHitResult);
		}
		case EVoxelActionType::Action2:
		{
			Toggle();
		}
		default: break;
	}
	return false;
}

void UVoxelTorch::Toggle()
{
	if(!bOn) TakeOn();
	else TakeOff();
}

void UVoxelTorch::TakeOn()
{
	bOn = true;
	RefreshData();
	if(GetAuxiliary<AVoxelTorchAuxiliary>())
	{
		GetAuxiliary<AVoxelTorchAuxiliary>()->SetLightVisible(bOn);
	}
}

void UVoxelTorch::TakeOff()
{
	bOn = false;
	RefreshData();
	if(GetAuxiliary<AVoxelTorchAuxiliary>())
	{
		GetAuxiliary<AVoxelTorchAuxiliary>()->SetLightVisible(bOn);
	}
}
