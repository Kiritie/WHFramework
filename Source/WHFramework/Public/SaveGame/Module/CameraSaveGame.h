// Copyright 2021 Sam Carey. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleSaveGame.h"
#include "Camera/CameraModuleTypes.h"
#include "CameraSaveGame.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UCameraSaveGame : public UModuleSaveGame
{
	GENERATED_BODY()
	
public:
	UCameraSaveGame();

public:
	virtual void OnCreate_Implementation(int32 InIndex) override;
	
	virtual void OnLoad_Implementation(EPhase InPhase) override;
	
	virtual void OnUnload_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation() override;
	
protected:
	UPROPERTY()
	FCameraModuleSaveData SaveData;

	UPROPERTY()
	FCameraModuleSaveData DefaultData;

public:
	virtual FSaveData* GetSaveData() override { return &SaveData; }

	virtual void SetSaveData(FSaveData* InSaveData) override { SaveData = InSaveData->CastRef<FCameraModuleSaveData>(); }

	virtual FSaveData* GetDefaultData() override { return &DefaultData; }

	virtual void SetDefaultData(FSaveData* InDefaultData) override { DefaultData = InDefaultData->CastRef<FCameraModuleSaveData>(); }
};
