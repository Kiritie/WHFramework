// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Auxiliary/VoxelTorchAuxiliary.h"
#include "Components/PointLightComponent.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Datas/VoxelData.h"

// Sets default values
AVoxelTorchAuxiliary::AVoxelTorchAuxiliary()
{
	LightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("LightComponent"));
	LightComponent->SetupAttachment(RootComponent);
	LightComponent->SetLightColor(FLinearColor(1, 0.75f, 0, 1));
	LightComponent->SetIntensity(50000.f);
	LightComponent->SetAttenuationRadius(1500.f);
	LightComponent->SetCastShadows(false);
}

// Called when the game starts or when spawned
void AVoxelTorchAuxiliary::BeginPlay()
{
	Super::BeginPlay();

}

void AVoxelTorchAuxiliary::Initialize(AVoxelChunk* InOwnerChunk, FIndex InVoxelIndex)
{
	Super::Initialize(InOwnerChunk, InVoxelIndex);

	LightComponent->SetRelativeLocation(FVector::UpVector * GetVoxelItem().GetData<UVoxelData>().GetFinalRange().Z * 0.5f * UVoxelModuleBPLibrary::GetWorldData().BlockSize);
}
