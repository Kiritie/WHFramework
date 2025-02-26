// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHActor.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "Voxel/VoxelModuleTypes.h"

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

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void OnInitialize_Implementation() override;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData() override;
	
	virtual void DestroyAuxiliary();

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UVoxelMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FPrimaryAssetId VoxelID;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	EVoxelScope VoxelScope;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	AVoxelAuxiliary* Auxiliary;

public:
	UVoxelMeshComponent* GetMeshComponent() const { return MeshComponent; }

	FPrimaryAssetId GetVoxelID() const { return VoxelID; }

	AVoxelAuxiliary* GetAuxiliary() const { return Auxiliary; }
};
