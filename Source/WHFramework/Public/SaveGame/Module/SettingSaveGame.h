// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ModuleSaveGame.h"
#include "Common/CommonTypes.h"
#include "Setting/SettingModuleTypes.h"

#include "SettingSaveGame.generated.h"

class UArchiveSaveGame;

/**
 * 常规游戏数据存取类
 */
UCLASS()
class WHFRAMEWORK_API USettingSaveGame : public UModuleSaveGame
{
	GENERATED_BODY()

public:
	USettingSaveGame();
	
public:
	virtual void OnCreate_Implementation(int32 InIndex) override;
	
	virtual void OnLoad_Implementation(EPhase InPhase) override;
	
	virtual void OnUnload_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation() override;

protected:
	UPROPERTY()
	FSettingModuleSaveData SaveData;
	
	UPROPERTY()
	FSettingModuleSaveData DefaultData;

public:
	virtual FSaveData* GetSaveData() override { return &SaveData; }

	virtual void SetSaveData(FSaveData* InSaveData) override { SaveData = InSaveData->CastRef<FSettingModuleSaveData>(); }

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetSaveData"))
	FSettingModuleSaveData& K2_GetSaveData() { return SaveData; }

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetSaveData"))
	void K2_SetSaveData(const FSettingModuleSaveData& InSaveData) { SaveData = InSaveData; }

	virtual FSaveData* GetDefaultData() override { return &DefaultData; }

	virtual void SetDefaultData(FSaveData* InDefaultData) override { DefaultData = InDefaultData->CastRef<FSettingModuleSaveData>(); }
};
