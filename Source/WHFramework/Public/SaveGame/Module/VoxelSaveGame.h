// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ModuleSaveGame.h"
#include "Voxel/VoxelModuleTypes.h"

#include "VoxelSaveGame.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UVoxelSaveGame : public UModuleSaveGame
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
	FVoxelModuleSaveData SaveData;

	UPROPERTY()
	FVoxelModuleSaveData DefaultData;

public:
	virtual FSaveData* GetSaveData() override { return &SaveData; }

	virtual void SetSaveData(FSaveData* InSaveData) override { SaveData = InSaveData->CastRef<FVoxelModuleSaveData>(); }

	virtual FSaveData* GetDefaultData() override { return &DefaultData; }

	virtual void SetDefaultData(FSaveData* InDefaultData) override { DefaultData = InDefaultData->CastRef<FVoxelModuleSaveData>(); }
};
