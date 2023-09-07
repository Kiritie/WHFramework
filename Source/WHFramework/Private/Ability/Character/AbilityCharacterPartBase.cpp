// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Character/AbilityCharacterPartBase.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "Character/Base/CharacterBase.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Datas/VoxelData.h"

UAbilityCharacterPartBase::UAbilityCharacterPartBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UAbilityCharacterPartBase::BeginPlay()
{
	Super::BeginPlay();
}

void UAbilityCharacterPartBase::UpdateVoxelOverlap()
{
	const auto OwnerCharacter = Cast<AAbilityCharacterBase>(GetOwnerCharacter());
	if(!OwnerCharacter || !OwnerCharacter->IsActive()) return;

	Super::UpdateVoxelOverlap();
}
