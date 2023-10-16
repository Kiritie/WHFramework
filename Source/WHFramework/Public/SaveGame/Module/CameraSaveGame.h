// Copyright 2021 Sam Carey. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModuleTypes.h"
#include "SaveGame/Base/SaveGameBase.h"
#include "CameraSaveGame.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UCameraSaveGame : public USaveGameBase
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

public:
	virtual FSaveData* GetSaveData() override { return &SaveData; }

	virtual void SetSaveData(FSaveData* InSaveData) override { SaveData = InSaveData->CastRef<FCameraModuleSaveData>(); }
};
