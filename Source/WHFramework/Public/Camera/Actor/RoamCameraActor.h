// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Camera/Actor/CameraActorBase.h"
#include "Voxel/Agent/VoxelAgentInterface.h"
#include "RoamCameraActor.generated.h"

UCLASS()
class WHFRAMEWORK_API ARoamCameraActor : public ACameraActorBase, public IVoxelAgentInterface
{
	GENERATED_BODY()

public:
	ARoamCameraActor();

public:
	virtual void OnInitialize_Implementation() override;

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	class USphereComponent* Sphere;
	
public:
	virtual void SetCameraCollisionMode(ECameraCollisionMode InCameraCollisionMode) override;

	//////////////////////////////////////////////////////////////////////////
	/// Voxel
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	FPrimaryAssetId GenerateVoxelID;

public:
	virtual FVector GetVoxelAgentLocation() const override { return GetActorLocation(); }

	virtual FPrimaryAssetId GetGenerateVoxelID() const override { return GenerateVoxelID; }

	virtual void SetGenerateVoxelID(const FPrimaryAssetId& InGenerateVoxelID) override { GenerateVoxelID = InGenerateVoxelID; }
};
