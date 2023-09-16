// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VoxelAuxiliary.h"
#include "VoxelTorchAuxiliary.generated.h"

class UVoxel;
class UPointLightComponent;

/**
 */
UCLASS()
class WHFRAMEWORK_API AVoxelTorchAuxiliary : public AVoxelAuxiliary
{
	GENERATED_BODY()
	
public:	
	AVoxelTorchAuxiliary();

public:
	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

public:
	virtual void SetLightVisible(bool bInVisible);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPointLightComponent* LightComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UParticleSystemComponent* EffectComponent;

public:
	UPointLightComponent* GetLightComponent() const { return LightComponent; }

	UParticleSystemComponent* GetEffectComponent() const { return EffectComponent; }
};
