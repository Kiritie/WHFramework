// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Auxiliary/VoxelTorchAuxiliary.h"
#include "Components/PointLightComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Voxel/Voxels/Data/VoxelTorchData.h"

// Sets default values
AVoxelTorchAuxiliary::AVoxelTorchAuxiliary()
{
	PointLight = CreateDefaultSubobject<UPointLightComponent>(FName("PointLight"));
	PointLight->SetupAttachment(RootComponent);
	PointLight->SetLightColor(FLinearColor(1.f, 0.8f, 0, 1.f));
	PointLight->SetIntensity(10000.f);
	PointLight->SetAttenuationRadius(1500.f);
	PointLight->SetCastShadows(true);

	ParticleSystem = CreateDefaultSubobject<UParticleSystemComponent>(FName("ParticleSystem"));
	ParticleSystem->SetupAttachment(PointLight);
}

void AVoxelTorchAuxiliary::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);

	if(PHASEC(InPhase, EPhase::All))
	{
		if(VoxelItem.IsValid())
		{
			const auto& VoxelData = VoxelItem.GetVoxelData<UVoxelTorchData>();
			ParticleSystem->SetTemplate(VoxelData.EffectAsset);
			ParticleSystem->SetRelativeScale3D(VoxelData.EffectScale);
			ParticleSystem->SetRelativeLocation(VoxelData.EffectOffset * UVoxelModule::Get().GetWorldData().BlockSize);
		
			PointLight->SetRelativeLocation(FVector::UpVector * VoxelItem.GetVoxelData().GetRange().Z * 0.5f * UVoxelModule::Get().GetWorldData().BlockSize);
		}
	}
}

void AVoxelTorchAuxiliary::SetLightVisible(bool bInVisible)
{
	PointLight->SetVisibility(bInVisible);
	ParticleSystem->SetVisibility(bInVisible);
}
