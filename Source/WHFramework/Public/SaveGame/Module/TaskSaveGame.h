// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Task/TaskModuleTypes.h"
#include "SaveGame/Base/SaveGameBase.h"

#include "TaskSaveGame.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UTaskSaveGame : public USaveGameBase
{
	GENERATED_BODY()

public:
	UTaskSaveGame();
	
public:
	virtual void OnCreate_Implementation(int32 InSaveIndex) override;
	
	virtual void OnLoad_Implementation(EPhase InPhase) override;
	
	virtual void OnUnload_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation() override;
	
protected:
	UPROPERTY()
	FTaskSaveData TaskSaveData;

public:
	virtual FSaveData* GetSaveData() override { return &TaskSaveData; }

	virtual void SetSaveData(FSaveData* InSaveData) override { TaskSaveData = InSaveData->CastRef<FTaskSaveData>(); }
};
