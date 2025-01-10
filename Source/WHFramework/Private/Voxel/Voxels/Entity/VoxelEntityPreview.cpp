// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Entity/VoxelEntityPreview.h"

#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Datas/VoxelData.h"

// Sets default values
AVoxelEntityPreview::AVoxelEntityPreview()
{
	MeshComponent->Initialize(EVoxelScope::Preview);
}

void AVoxelEntityPreview::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);
}

void AVoxelEntityPreview::SetMaterialColor(FLinearColor InColor)
{
	if(UMaterialInstanceDynamic* MatInst = Cast<UMaterialInstanceDynamic>(MeshComponent->GetMaterial(0)))
	{
		MatInst->SetVectorParameterValue(TEXT("Color"), InColor);
	}
}
