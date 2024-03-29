// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "SaveGame/Base/SaveGameBase.h"

#include "ParameterSaveGame.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UParameterSaveGame : public USaveGameBase
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
	FParameterSaveData ParameterSaveData;

public:
	virtual FSaveData* GetSaveData() override { return &ParameterSaveData; }

	virtual void SetSaveData(FSaveData* InSaveData) override { ParameterSaveData = InSaveData->CastRef<FParameterSaveData>(); }
};
