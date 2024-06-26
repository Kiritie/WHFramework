// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Auxiliary/VoxelTorchAuxiliary.h"
#include "Components/PointLightComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Voxel/VoxelModule.h"
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

void AVoxelTorchAuxiliary::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);

	if(PHASEC(InPhase, EPhase::All))
	{
		if(VoxelItem.IsValid())
		{
			const auto& VoxelData = VoxelItem.GetVoxelData<UVoxelTorchData>();
			EffectComponent->SetTemplate(VoxelData.EffectAsset);
			EffectComponent->SetRelativeScale3D(VoxelData.EffectScale);
			EffectComponent->SetRelativeLocation(VoxelData.EffectOffset * UVoxelModule::Get().GetWorldData().BlockSize);
		
			LightComponent->SetRelativeLocation(FVector::UpVector * VoxelItem.GetVoxelData().GetRange().Z * 0.5f * UVoxelModule::Get().GetWorldData().BlockSize);
		}
	}
}

void AVoxelTorchAuxiliary::SetLightVisible(bool bInVisible)
{
	LightComponent->SetVisibility(bInVisible);
	EffectComponent->SetVisibility(bInVisible);
}
