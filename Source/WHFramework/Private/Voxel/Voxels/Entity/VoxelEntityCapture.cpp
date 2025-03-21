// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Entity/VoxelEntityCapture.h"

#include "Asset/AssetModuleStatics.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Voxels/Data/VoxelData.h"

// Sets default values
AVoxelEntityCapture::AVoxelEntityCapture()
{
	VoxelScope = EVoxelScope::Capture;
}

void AVoxelEntityCapture::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);

	const UVoxelData& VoxelData = VoxelItem.GetVoxelData();
	const FVector Range = VoxelData.GetRange();
	const float TmpNum = (Range.X + Range.Y + Range.Z) / 3;
	SetActorScale3D(FVector(TmpNum / Range.X, TmpNum / Range.Y, TmpNum / Range.Z));
}
