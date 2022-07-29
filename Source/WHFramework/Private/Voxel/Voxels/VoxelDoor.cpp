// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/VoxelDoor.h"

#include "Kismet/GameplayStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"

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

void UVoxelDoor::OpenOrClose()
{
	if(IsOpened()) OpenTheDoor();
	else CloseTheDoor();
}

void UVoxelDoor::OpenTheDoor()
{
	bOpened = true;
	Rotation += FRotator(0, -90, 0);
	Scale = FVector(1, 1, 1);
	Owner->Generate();
	if(GetData().OperationSounds.Num() > 0)
	{
		UGameplayStatics::PlaySoundAtLocation(this, GetData().OperationSounds[0], Owner->IndexToLocation(Index));
	}
	RefreshData();
}

void UVoxelDoor::CloseTheDoor()
{
	bOpened = false;
	Rotation += FRotator(0, 90, 0);
	Scale = FVector(1, 1, 1);
	Owner->Generate();
	if(GetData().OperationSounds.Num() > 1)
	{
		UGameplayStatics::PlaySoundAtLocation(this, GetData().OperationSounds[1], Owner->IndexToLocation(Index));
	}
	RefreshData();
}

void UVoxelDoor::OnTargetHit(IVoxelAgentInterface* InTarget, const FVoxelHitResult& InHitResult)
{
	Super::OnTargetHit(InTarget, InHitResult);
}

void UVoxelDoor::OnTargetEnter(IVoxelAgentInterface* InTarget, const FVoxelHitResult& InHitResult)
{
	Super::OnTargetEnter(InTarget, InHitResult);
}

void UVoxelDoor::OnTargetStay(IVoxelAgentInterface* InTarget, const FVoxelHitResult& InHitResult)
{
	Super::OnTargetStay(InTarget, InHitResult);
}

void UVoxelDoor::OnTargetExit(IVoxelAgentInterface* InTarget, const FVoxelHitResult& InHitResult)
{
	Super::OnTargetExit(InTarget, InHitResult);
}

bool UVoxelDoor::OnMouseDown(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult)
{
	switch (InMouseButton)
	{
		case EMouseButton::Left:
		{
			return Super::OnMouseDown(InMouseButton, InHitResult);
		}
		case EMouseButton::Right:
		{
			FHitResult hitResult;
			if (!UVoxelModuleBPLibrary::VoxelTraceSingle(InHitResult.VoxelItem, Owner->IndexToLocation(Index), hitResult))
			{
				OpenOrClose();
				return true;
			}
			break;
		}
		default: break;
	}
	return false;
}

bool UVoxelDoor::OnMouseUp(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult)
{
	return Super::OnMouseUp(InMouseButton, InHitResult);
}

bool UVoxelDoor::OnMouseHold(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult)
{
	return Super::OnMouseHold(InMouseButton, InHitResult);
}

void UVoxelDoor::OnMouseHover(const FVoxelHitResult& InHitResult)
{
	Super::OnMouseHover(InHitResult);
}
