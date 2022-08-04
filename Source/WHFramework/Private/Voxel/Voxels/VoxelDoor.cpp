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

void UVoxelDoor::OnDespawn_Implementation()
{
	Super::OnDespawn_Implementation();

	bOpened = false;
}

void UVoxelDoor::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(Ar.ArIsSaveGame)
	{
		if(Ar.IsLoading())
		{
			Ar << bOpened;
		}
		else if(Ar.IsSaving())
		{
			Ar << bOpened;
		}
	}
}

void UVoxelDoor::LoadData(FSaveData* InSaveData, bool bForceMode)
{
	Super::LoadData(InSaveData, bForceMode);
}

FSaveData* UVoxelDoor::ToData()
{
	return Super::ToData();
}

void UVoxelDoor::OnGenerate(IVoxelAgentInterface* InAgent)
{
	Super::OnGenerate(InAgent);
}

void UVoxelDoor::OnReplace(IVoxelAgentInterface* InAgent, const FVoxelItem& InOldVoxelItem)
{
	Super::OnReplace(InAgent, InOldVoxelItem);
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

bool UVoxelDoor::OnActionTrigger(IVoxelAgentInterface* InAgent, EVoxelActionType InActionType, const FVoxelHitResult& InHitResult)
{
	switch (InActionType)
	{
		case EVoxelActionType::Action1:
		{
			return Super::OnActionTrigger(InAgent, InActionType, InHitResult);
		}
		case EVoxelActionType::Action2:
		{
			FHitResult hitResult;
			if (!UVoxelModuleBPLibrary::VoxelTraceSingle(InHitResult.VoxelItem, Owner->IndexToLocation(Index), hitResult))
			{
				Toggle();
				return true;
			}
			break;
		}
		default: break;
	}
	return false;
}

void UVoxelDoor::Toggle()
{
	if(!bOpened) Open();
	else Close();
}

void UVoxelDoor::Open()
{
	bOpened = true;
	Rotation += FRotator(0, -90, 0);
	Scale = FVector(1, 1, 1);
	Owner->Generate();
	UAudioModuleBPLibrary::PlaySoundAtLocation(GetData<UVoxelDoorData>().OpenSound, Owner->IndexToLocation(Index));
	RefreshData();
}

void UVoxelDoor::Close()
{
	bOpened = false;
	Rotation += FRotator(0, 90, 0);
	Scale = FVector(1, 1, 1);
	Owner->Generate();
	UAudioModuleBPLibrary::PlaySoundAtLocation(GetData<UVoxelDoorData>().CloseSound, Owner->IndexToLocation(Index));
	RefreshData();
}
