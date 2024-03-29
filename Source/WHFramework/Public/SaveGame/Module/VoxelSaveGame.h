// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Voxel/VoxelModuleTypes.h"
#include "SaveGame/Base/SaveGameBase.h"

#include "VoxelSaveGame.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UVoxelSaveGame : public USaveGameBase
{
	GENERATED_BODY()

public:
	UVoxelSaveGame();
	
public:
	virtual void OnCreate_Implementation(int32 InIndex) override;
	
	virtual void OnLoad_Implementation(EPhase InPhase) override;
	
	virtual void OnUnload_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation() override;
	
protected:
	UPROPERTY()
	FVoxelSaveData VoxelSaveData;

public:
	virtual FSaveData* GetSaveData() override { return &VoxelSaveData; }

	virtual void SetSaveData(FSaveData* InSaveData) override { VoxelSaveData = InSaveData->CastRef<FVoxelSaveData>(); }
};
