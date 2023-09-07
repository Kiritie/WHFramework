// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Base/CharacterPartBase.h"

#include "Character/Base/CharacterBase.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Datas/VoxelData.h"

UCharacterPartBase::UCharacterPartBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	
	UPrimitiveComponent::SetCollisionProfileName(TEXT("CharacterPart"));
	InitBoxExtent(FVector(15, 15, 15));

	LastOverlapVoxel = FVoxelItem();
}

void UCharacterPartBase::BeginPlay()
{
	Super::BeginPlay();
}

void UCharacterPartBase::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	UpdateVoxelOverlap();
}

void UCharacterPartBase::UpdateVoxelOverlap()
{
	if(!GetOwnerCharacter()) return;
	
	if(AVoxelChunk* Chunk = Cast<AVoxelChunk>(GetOwnerCharacter()->Execute_GetContainer(GetOwnerCharacter()).GetObject()))
	{
		const FVoxelItem& VoxelItem = Chunk->GetVoxelItem(Chunk->LocationToIndex(GetComponentLocation()), true);
		if(VoxelItem != LastOverlapVoxel)
		{
			const FVoxelHitResult VoxelHitResult = FVoxelHitResult(VoxelItem, GetComponentLocation(), GetOwnerCharacter()->GetMoveDirection(false));
			if(VoxelItem.IsValid())
			{
				if(LastOverlapVoxel != VoxelItem)
				{
					if(LastOverlapVoxel.IsValid())
					{
						OnExitVoxel(LastOverlapVoxel.GetVoxel(), VoxelHitResult);
						LastOverlapVoxel = FVoxelItem::Empty;
					}
					LastOverlapVoxel = VoxelItem;
					OnEnterVoxel(VoxelItem.GetVoxel(), VoxelHitResult);
				}
				if(LastOverlapVoxel.IsValid())
				{
					OnStayVoxel(LastOverlapVoxel.GetVoxel(), VoxelHitResult);
				}
			}
			else if(LastOverlapVoxel.IsValid())
			{
				OnExitVoxel(LastOverlapVoxel.GetVoxel(), VoxelHitResult);
				LastOverlapVoxel = FVoxelItem::Empty;
			}
		}
	}
}

void UCharacterPartBase::OnHitVoxel(UVoxel& InVoxel, const FVoxelHitResult& InHitResult)
{
	InVoxel.OnAgentHit(GetOwnerCharacter(), InHitResult);
}

void UCharacterPartBase::OnEnterVoxel(UVoxel& InVoxel, const FVoxelHitResult& InHitResult)
{
	InVoxel.OnAgentEnter(GetOwnerCharacter(), InHitResult);
}

void UCharacterPartBase::OnStayVoxel(UVoxel& InVoxel, const FVoxelHitResult& InHitResult)
{
	InVoxel.OnAgentStay(GetOwnerCharacter(), InHitResult);
}

void UCharacterPartBase::OnExitVoxel(UVoxel& InVoxel, const FVoxelHitResult& InHitResult)
{
	InVoxel.OnAgentExit(GetOwnerCharacter(), InHitResult);
}

ACharacterBase* UCharacterPartBase::GetOwnerCharacter() const
{
	return Cast<ACharacterBase>(GetOwner());
}
