// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Entity/VoxelEntityPreview.h"

#include "Asset/AssetModuleBPLibrary.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Datas/VoxelData.h"

// Sets default values
AVoxelEntityPreview::AVoxelEntityPreview()
{
	MeshComponent->Initialize(EVoxelMeshNature::Preview);
}

void AVoxelEntityPreview::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);

	const UVoxelData& VoxelData = UAssetModuleBPLibrary::LoadPrimaryAssetRef<UVoxelData>(VoxelID);
	const FVector Range = VoxelData.GetRange();
	const float TmpNum = (Range.X + Range.Y + Range.Z) / 3;
	SetActorScale3D(FVector(TmpNum / Range.X, TmpNum / Range.Y, TmpNum / Range.Z));
}
