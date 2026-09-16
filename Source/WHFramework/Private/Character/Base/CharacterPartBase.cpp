// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/Base/CharacterPartBase.h"

#include "Character/Base/CharacterBase.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelSectionKey.h"

UCharacterPartBase::UCharacterPartBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
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
	ACharacterBase* OwnerCharacter = GetOwnerCharacter();
	UVoxelModule* VoxelModule = UVoxelModule::GetPtr();
	if (!OwnerCharacter || !VoxelModule || !VoxelModule->IsReady())
	{
		OverlappingVoxel = FVoxelItem();
		return;
	}

	FIntVector BlockIndex;
	if (!VoxelCoord::FromWorld(GetComponentLocation(), VoxelModule->BlockSize(), BlockIndex) ||
	    !UVoxelModuleStatics::GetVoxelBlock(this, BlockIndex, OverlappingVoxel))
	{
		OverlappingVoxel = FVoxelItem();
	}
}

ACharacterBase* UCharacterPartBase::GetOwnerCharacter(TSubclassOf<ACharacterBase> InClass) const
{
	return GetDeterminesOutputObject(Cast<ACharacterBase>(GetOwner()), InClass);
}
