// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ModuleSaveGame.h"
#include "Input/InputModuleTypes.h"

#include "InputSaveGame.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UInputSaveGame : public UModuleSaveGame
{
	GENERATED_BODY()

public:
	UInputSaveGame();
	
public:
	virtual void OnCreate_Implementation(int32 InIndex) override;
	
	virtual void OnLoad_Implementation(EPhase InPhase) override;
	
	virtual void OnUnload_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation() override;
	
protected:
	UPROPERTY()
	FInputModuleSaveData SaveData;

	UPROPERTY()
	FInputModuleSaveData DefaultData;

public:
	virtual FSaveData* GetSaveData() override { return &SaveData; }

	virtual void SetSaveData(FSaveData* InSaveData) override { SaveData = InSaveData->CastRef<FInputModuleSaveData>(); }

	virtual FSaveData* GetDefaultData() override { return &DefaultData; }

	virtual void SetDefaultData(FSaveData* InDefaultData) override { DefaultData = InDefaultData->CastRef<FInputModuleSaveData>(); }
};
