// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Entity/VoxelEntityPreview.h"

#include "Asset/AssetModuleBPLibrary.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Datas/VoxelData.h"

// Sets default values
AVoxelEntityPreview::AVoxelEntityPreview()
{
	MeshComponent->Initialize(EVoxelMeshNature::Preview);
	MeshComponent->SetRelativeRotation(FRotator(0.f, 45.f, 0.f));
}

void AVoxelEntityPreview::Initialize(FPrimaryAssetId InVoxelID)
{
	Super::Initialize(InVoxelID);

	const UVoxelData& voxelData = UAssetModuleBPLibrary::LoadPrimaryAssetRef<UVoxelData>(InVoxelID);
	const FVector range = voxelData.GetRange();
	const float tmpNum = (range.X + range.Y + range.Z) / 3;
	SetActorScale3D(FVector(tmpNum / range.X, tmpNum / range.Y, tmpNum / range.Z));
}
