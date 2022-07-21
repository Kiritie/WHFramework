// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "SaveGame/SaveGameModuleTypes.h"
#include "SaveGame/Base/SaveGameBase.h"

#include "GeneralSaveGame.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UGeneralSaveGame : public USaveGameBase
{
	GENERATED_BODY()

public:
	UGeneralSaveGame();
	
public:
	virtual void OnCreate_Implementation(int32 InSaveIndex) override;
	
	virtual void OnLoad_Implementation(bool bForceMode) override;
	
	virtual void OnUnload_Implementation(bool bForceMode) override;

	virtual void OnRefresh_Implementation() override;
	
protected:
	UPROPERTY()
	FGeneralSaveData GeneralSaveData;

public:
	virtual FSaveData* GetSaveData() override { return &GeneralSaveData; }

	virtual void SetSaveData(FSaveData* InSaveData) override { GeneralSaveData = InSaveData->CastRef<FGeneralSaveData>(); }
};
