// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Base/CharacterPartBase.h"

#include "Character/Base/CharacterBase.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Voxels/Voxel.h"

UCharacterPartBase::UCharacterPartBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	
	UPrimitiveComponent::SetCollisionProfileName(TEXT("CharacterPart"));
	InitBoxExtent(FVector(15, 15, 15));

	OverlappingVoxel = FVoxelItem();
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
	
	if(AVoxelChunk* Chunk = Cast<AVoxelChunk>(ISceneActorInterface::Execute_GetContainer(GetOwnerCharacter()).GetObject()))
	{
		const FVoxelItem& StayingVoxel = Chunk->GetVoxelComplex(Chunk->LocationToIndex(GetComponentLocation()), true);
		const FVoxelHitResult VoxelHitResult = FVoxelHitResult(StayingVoxel, GetComponentLocation(), GetOwnerCharacter()->GetMoveDirection());
		if(StayingVoxel != OverlappingVoxel)
		{
			if(StayingVoxel.IsValid())
			{
				if(OverlappingVoxel.IsValid())
				{
					OnExitVoxel(OverlappingVoxel.GetVoxel(), VoxelHitResult);
				}
				OverlappingVoxel = StayingVoxel;
				OnEnterVoxel(StayingVoxel.GetVoxel(), VoxelHitResult);
			}
			else if(OverlappingVoxel.IsValid())
			{
				OnExitVoxel(OverlappingVoxel.GetVoxel(), VoxelHitResult);
				OverlappingVoxel = FVoxelItem::Empty;
			}
		}
		else if(OverlappingVoxel.IsValid())
		{
			OnStayVoxel(OverlappingVoxel.GetVoxel(), VoxelHitResult);
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

ACharacterBase* UCharacterPartBase::GetOwnerCharacter(TSubclassOf<ACharacterBase> InClass) const
{
	return GetDeterminesOutputObject(Cast<ACharacterBase>(GetOwner()), InClass);
}
