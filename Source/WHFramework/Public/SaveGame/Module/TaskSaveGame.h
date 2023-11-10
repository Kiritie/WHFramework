// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ModuleSaveGame.h"
#include "Task/TaskModuleTypes.h"

#include "TaskSaveGame.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UTaskSaveGame : public UModuleSaveGame
{
	GENERATED_BODY()

public:
	UTaskSaveGame();
	
public:
	virtual void OnCreate_Implementation(int32 InIndex) override;
	
	virtual void OnLoad_Implementation(EPhase InPhase) override;
	
	virtual void OnUnload_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation() override;
	
protected:
	UPROPERTY()
	FTaskModuleSaveData SaveData;

	UPROPERTY()
	FTaskModuleSaveData DefaultData;

public:
	virtual FSaveData* GetSaveData() override { return &SaveData; }

	virtual void SetSaveData(FSaveData* InSaveData) override { SaveData = InSaveData->CastRef<FTaskModuleSaveData>(); }

	virtual FSaveData* GetDefaultData() override { return &DefaultData; }

	virtual void SetDefaultData(FSaveData* InDefaultData) override { DefaultData = InDefaultData->CastRef<FTaskModuleSaveData>(); }
};
