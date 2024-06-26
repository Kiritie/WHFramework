// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ModuleSaveGame.h"
#include "Parameter/ParameterModuleTypes.h"

#include "ParameterSaveGame.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UParameterSaveGame : public UModuleSaveGame
{
	GENERATED_BODY()

public:
	UParameterSaveGame();
	
public:
	virtual void OnCreate_Implementation(int32 InIndex) override;
	
	virtual void OnLoad_Implementation(EPhase InPhase) override;
	
	virtual void OnUnload_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation() override;
	
protected:
	UPROPERTY()
	FParameterModuleSaveData SaveData;

	UPROPERTY()
	FParameterModuleSaveData DefaultData;

public:
	virtual FSaveData* GetSaveData() override { return &SaveData; }

	virtual void SetSaveData(FSaveData* InSaveData) override { SaveData = InSaveData->CastRef<FParameterModuleSaveData>(); }

	virtual FSaveData* GetDefaultData() override { return &DefaultData; }

	virtual void SetDefaultData(FSaveData* InDefaultData) override { DefaultData = InDefaultData->CastRef<FParameterModuleSaveData>(); }
};
