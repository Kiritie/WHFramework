// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Auxiliary/VoxelTorchAuxiliary.h"
#include "Components/PointLightComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Datas/VoxelTorchData.h"

// Sets default values
AVoxelTorchAuxiliary::AVoxelTorchAuxiliary()
{
	LightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("LightComponent"));
	LightComponent->SetupAttachment(RootComponent);
	LightComponent->SetLightColor(FLinearColor(1.f, 0.8f, 0, 1.f));
	LightComponent->SetIntensity(10000.f);
	LightComponent->SetAttenuationRadius(1500.f);
	LightComponent->SetCastShadows(true);

	EffectComponent = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("EffectComponent"));
	EffectComponent->SetupAttachment(LightComponent);
}

void AVoxelTorchAuxiliary::BeginPlay()
{
	Super::BeginPlay();

}

void AVoxelTorchAuxiliary::Initialize(FVoxelItem InVoxelItem)
{
	Super::Initialize(InVoxelItem);

	if(InVoxelItem.IsValid())
	{
		EffectComponent->SetTemplate(InVoxelItem.GetVoxelData<UVoxelTorchData>().EffectAsset);
		EffectComponent->SetRelativeScale3D(InVoxelItem.GetVoxelData<UVoxelTorchData>().EffectScale);
		EffectComponent->SetRelativeLocation(InVoxelItem.GetVoxelData<UVoxelTorchData>().EffectOffset * AVoxelModule::Get()->GetWorldData().BlockSize);
		
		LightComponent->SetRelativeLocation(FVector::UpVector * InVoxelItem.GetVoxelData().GetRange().Z * 0.5f * AVoxelModule::Get()->GetWorldData().BlockSize);
	}
}

void AVoxelTorchAuxiliary::SetLightVisible(bool bNewVisible)
{
	LightComponent->SetVisibility(bNewVisible);
	EffectComponent->SetVisibility(bNewVisible);
}
