// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VoxelAuxiliary.h"
#include "VoxelTorchAuxiliary.generated.h"

class UVoxel;
class UPointLightComponent;

/**
 * ��Ѹ�����
 */
UCLASS()
class WHFRAMEWORK_API AVoxelTorchAuxiliary : public AVoxelAuxiliary
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AVoxelTorchAuxiliary();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPointLightComponent* LightComponent;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	virtual void Initialize(AVoxelChunk* InOwnerChunk, FIndex InVoxelIndex) override;
};
