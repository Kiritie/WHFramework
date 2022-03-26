// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/VoxelWater.h"

#include "World/Chunk.h"
#include "World/VoxelModule.h"
#include "Character/AbilityCharacter.h"
#include "Character/AbilityCharacterPart.h"
#include "Character/Player/DWPlayerCharacter.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"

UVoxelWater::UVoxelWater()
{
	
}

void UVoxelWater::LoadData(const FString& InValue)
{
	Super::LoadData(InValue);
}

FString UVoxelWater::ToData()
{
	return Super::ToData();
}

void UVoxelWater::OnTargetHit(ACharacterBase* InTarget, const FVoxelHitResult& InHitResult)
{
	Super::OnTargetHit(InTarget, InHitResult);
}

void UVoxelWater::OnTargetEnter(UAbilityCharacterPart* InTarget, const FVoxelHitResult& InHitResult)
{
	Super::OnTargetEnter(InTarget, InHitResult);
	
	if(ACharacterBase* Character = InTarget->GetOwnerCharacter())
	{
		switch (InTarget->GetCharacterPartType())
		{
			case ECharacterPartType::Chest:
			{
				Character->Swim();
				break;
			}
			case ECharacterPartType::Neck:
			{
				Character->UnFloat();
				break;
			}
			default: break;
		}
	}
}

void UVoxelWater::OnTargetStay(UAbilityCharacterPart* InTarget, const FVoxelHitResult& InHitResult)
{
	Super::OnTargetStay(InTarget, InHitResult);
}

void UVoxelWater::OnTargetExit(UAbilityCharacterPart* InTarget, const FVoxelHitResult& InHitResult)
{
	Super::OnTargetExit(InTarget, InHitResult);

	if(ACharacterBase* Character = InTarget->GetOwnerCharacter())
	{
		switch (InTarget->GetCharacterPartType())
		{
			case ECharacterPartType::Chest:
			{
				if(InHitResult.VoxelItem.GetVoxelData().VoxelType != EVoxelType::Water)
				{
					Character->UnSwim();
				}
				break;
			}
			case ECharacterPartType::Neck:
			{
				if(InHitResult.VoxelItem.GetVoxelData().VoxelType != EVoxelType::Water)
				{
					Character->Float(Owner->IndexToLocation(Index).Z + AVoxelModule::GetWorldData().BlockSize);
				}
				break;
			}
			default: break;
		}
	}
}

bool UVoxelWater::OnMouseDown(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult)
{
	return false;
}

bool UVoxelWater::OnMouseUp(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult)
{
	return Super::OnMouseUp(InMouseButton, InHitResult);
}

bool UVoxelWater::OnMouseHold(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult)
{
	return Super::OnMouseHold(InMouseButton, InHitResult);
}

void UVoxelWater::OnMouseHover(const FVoxelHitResult& InHitResult)
{
	Super::OnMouseHover(InHitResult);
}
