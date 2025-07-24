// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHActor.h"
#include "Voxel/VoxelModuleTypes.h"

#include "VoxelPrefab.generated.h"

class AVoxelAuxiliary;
class UVoxelData;
class UVoxelMeshComponent;

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API AVoxelPrefab : public AWHActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AVoxelPrefab();

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

public:
	virtual void CreateMesh();

	virtual void BuildMesh();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TMap<EVoxelNature, UVoxelMeshComponent*> MeshComponents;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TMap<FIndex, FVoxelItem> VoxelMap;

public:
	void SpawnMeshComponents();

	void DestroyMeshComponents();

	AVoxelAuxiliary* SpawnAuxiliary(FVoxelItem& InVoxelItem);

	void DestroyAuxiliary(FVoxelItem& InVoxelItem);

	void DestroyAuxiliarys();

public:
	bool HasVoxel(FIndex InIndex);

	bool HasVoxel(int32 InX, int32 InY, int32 InZ);

	FVoxelItem& GetVoxel(FIndex InIndex);

	FVoxelItem& GetVoxel(int32 InX, int32 InY, int32 InZ);

	void SetVoxel(FIndex InIndex, const FVoxelItem& InVoxelItem);

	void SetVoxel(int32 InX, int32 InY, int32 InZ, const FVoxelItem& InVoxelItem);

	UVoxelMeshComponent* GetMeshComponent(EVoxelNature InVoxelNature);
};
