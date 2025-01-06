// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Asset/Primary/PrimaryEntityInterface.h"
#include "Common/Base/WHActor.h"
#include "Voxel/VoxelModuleTypes.h"

#include "VoxelAuxiliary.generated.h"

class AVoxelChunk;
class UVoxel;

/**
 * ���ظ�����
 */
UCLASS()
class WHFRAMEWORK_API AVoxelAuxiliary : public AWHActor, public IPrimaryEntityInterface
{
	GENERATED_BODY()
	
public:	
	AVoxelAuxiliary();

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVoxelItem VoxelItem;

public:
	FVoxelItem& GetVoxelItem(bool bRefresh = false);

	virtual FPrimaryAssetId GetAssetID_Implementation() const override { return VoxelItem.ID; }

	virtual void SetAssetID_Implementation(const FPrimaryAssetId& InID) override { VoxelItem.ID = InID; }
};
