// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Entity/VoxelEntityCapture.h"

#include "Asset/AssetModuleStatics.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Datas/VoxelData.h"

// Sets default values
AVoxelEntityCapture::AVoxelEntityCapture()
{
	MeshComponent->Initialize(EVoxelScope::Capture);
}

void AVoxelEntityCapture::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);

	const UVoxelData& VoxelData = UAssetModuleStatics::LoadPrimaryAssetRef<UVoxelData>(VoxelID);
	const FVector Range = VoxelData.GetRange();
	const float TmpNum = (Range.X + Range.Y + Range.Z) / 3;
	SetActorScale3D(FVector(TmpNum / Range.X, TmpNum / Range.Y, TmpNum / Range.Z));
}
