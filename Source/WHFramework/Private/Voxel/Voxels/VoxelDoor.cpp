// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/VoxelDoor.h"
#include "World/Chunk.h"
#include "World/VoxelModule.h"
#include "Kismet/GameplayStatics.h"

UVoxelDoor::UVoxelDoor()
{
	
}

void UVoxelDoor::LoadData(const FString& InValue)
{
	Super::LoadData(InValue);
	
	TArray<FString> data;
	InValue.ParseIntoArray(data, TEXT(";"));
	
	SetOpened((bool)FCString::Atoi(*data[3]));
}

FString UVoxelDoor::ToData()
{
	return Super::ToData() + FString::Printf(TEXT(";%d"), (int32)IsOpened());
}

void UVoxelDoor::OpenOrClose()
{
	if(IsOpened()) OpenTheDoor();
	else CloseTheDoor();
}

void UVoxelDoor::OpenTheDoor()
{
	SetOpened(false);
	Rotation += FRotator(0, -90, 0);
	Scale = FVector(1, 1, 1);
	Owner->Generate();
	if(GetData<UVoxelAssetBase>().OperationSounds.Num() > 0)
	{
		UGameplayStatics::PlaySoundAtLocation(this, GetData<UVoxelAssetBase>().OperationSounds[0], Owner->IndexToLocation(Index));
	}
}

void UVoxelDoor::CloseTheDoor()
{
	SetOpened(true);
	Rotation += FRotator(0, 90, 0);
	Scale = FVector(1, 1, 1);
	Owner->Generate();
	if(GetData<UVoxelAssetBase>().OperationSounds.Num() > 1)
	{
		UGameplayStatics::PlaySoundAtLocation(this, GetData<UVoxelAssetBase>().OperationSounds[1], Owner->IndexToLocation(Index));
	}
}

bool UVoxelDoor::IsOpened() const
{
	return Params[FName("bOpened")].GetBooleanValue();
}

void UVoxelDoor::SetOpened(bool InValue)
{
	Params[FName("bOpened")].SetBooleanValue(InValue);
}

void UVoxelDoor::OnSpawn_Implementation()
{
	Params.Add(FName("bOpened"), FParameter::MakeBoolean(false));
}

void UVoxelDoor::OnTargetHit(ACharacterBase* InTarget, const FVoxelHitResult& InHitResult)
{
	Super::OnTargetHit(InTarget, InHitResult);
}

void UVoxelDoor::OnTargetEnter(UAbilityCharacterPart* InTarget, const FVoxelHitResult& InHitResult)
{
	Super::OnTargetEnter(InTarget, InHitResult);
}

void UVoxelDoor::OnTargetStay(UAbilityCharacterPart* InTarget, const FVoxelHitResult& InHitResult)
{
	Super::OnTargetStay(InTarget, InHitResult);
}

void UVoxelDoor::OnTargetExit(UAbilityCharacterPart* InTarget, const FVoxelHitResult& InHitResult)
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
			if (!AVoxelModule::Get()->VoxelTraceSingle(InHitResult.VoxelItem, Owner->IndexToLocation(Index), hitResult))
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
