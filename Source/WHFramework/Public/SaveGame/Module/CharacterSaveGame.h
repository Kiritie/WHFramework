// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ModuleSaveGame.h"
#include "Character/CharacterModuleTypes.h"

#include "CharacterSaveGame.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UCharacterSaveGame : public UModuleSaveGame
{
	GENERATED_BODY()

public:
	UCharacterSaveGame();
	
public:
	virtual void OnCreate_Implementation(int32 InIndex) override;
	
	virtual void OnLoad_Implementation(EPhase InPhase) override;
	
	virtual void OnUnload_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation() override;
	
protected:
	UPROPERTY()
	FCharacterModuleSaveData SaveData;

	UPROPERTY()
	FCharacterModuleSaveData DefaultData;

public:
	virtual FSaveData* GetSaveData() override { return &SaveData; }

	virtual void SetSaveData(FSaveData* InSaveData) override { SaveData = InSaveData->CastRef<FCharacterModuleSaveData>(); }

	virtual FSaveData* GetDefaultData() override { return &DefaultData; }

	virtual void SetDefaultData(FSaveData* InDefaultData) override { DefaultData = InDefaultData->CastRef<FCharacterModuleSaveData>(); }
};
