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

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPointLightComponent* LightComponent;

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Initialize(FIndex InVoxelIndex) override;

public:
	UPointLightComponent* GetLightComponent() const { return LightComponent; }
};
