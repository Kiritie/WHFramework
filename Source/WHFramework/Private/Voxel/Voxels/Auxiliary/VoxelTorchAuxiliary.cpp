// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Auxiliary/VoxelTorchAuxiliary.h"
#include "Components/PointLightComponent.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Datas/VoxelData.h"

// Sets default values
AVoxelTorchAuxiliary::AVoxelTorchAuxiliary()
{
	LightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("LightComponent"));
	LightComponent->SetupAttachment(RootComponent);
	LightComponent->SetLightColor(FLinearColor(1, 0.8f, 0, 1));
	LightComponent->SetIntensity(10000.f);
	LightComponent->SetAttenuationRadius(1500.f);
	LightComponent->SetCastShadows(true);
}

void AVoxelTorchAuxiliary::BeginPlay()
{
	Super::BeginPlay();

}

void AVoxelTorchAuxiliary::Initialize(FIndex InVoxelIndex)
{
	Super::Initialize(InVoxelIndex);

	LightComponent->SetRelativeLocation(FVector::UpVector * GetVoxelItem().GetVoxelData().GetRange().Z * 0.5f * UVoxelModuleBPLibrary::GetWorldData().BlockSize);
}
