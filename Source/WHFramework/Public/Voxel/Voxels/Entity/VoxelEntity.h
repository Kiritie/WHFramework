// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Global/Base/WHActor.h"
#include "ObjectPool/ObjectPoolInterface.h"

#include "VoxelEntity.generated.h"

class AVoxelAuxiliary;
class UVoxelData;
class UVoxelMeshComponent;

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API AVoxelEntity : public AWHActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AVoxelEntity();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UVoxelMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FPrimaryAssetId VoxelID;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	AVoxelAuxiliary* Auxiliary;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	virtual void Initialize(FPrimaryAssetId InVoxelID);

public:
	virtual int32 GetLimit_Implementation() const override { return 1000; }

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation() override;

public:
	UVoxelMeshComponent* GetMeshComponent() const { return MeshComponent; }

	FPrimaryAssetId GetVoxelID() const { return VoxelID; }

	AVoxelAuxiliary* GetAuxiliary() const { return Auxiliary; }
};
