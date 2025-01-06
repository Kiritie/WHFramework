// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VoxelInteractAuxiliary.h"
#include "VoxelTorchAuxiliary.generated.h"

class UVoxel;
class UPointLightComponent;

/**
 */
UCLASS()
class WHFRAMEWORK_API AVoxelTorchAuxiliary : public AVoxelInteractAuxiliary
{
	GENERATED_BODY()
	
public:	
	AVoxelTorchAuxiliary();

public:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

public:
	virtual void SetLightVisible(bool bInVisible);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPointLightComponent* PointLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UParticleSystemComponent* ParticleSystem;

public:
	UPointLightComponent* GetPointLight() const { return PointLight; }

	UParticleSystemComponent* GetParticleSystem() const { return ParticleSystem; }
};
