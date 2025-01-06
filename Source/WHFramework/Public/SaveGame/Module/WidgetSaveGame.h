// Copyright 2021 Sam Carey. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleSaveGame.h"
#include "Widget/WidgetModuleTypes.h"
#include "WidgetSaveGame.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetSaveGame : public UModuleSaveGame
{
	GENERATED_BODY()
	
public:
	UWidgetSaveGame();

public:
	virtual void OnCreate_Implementation(int32 InIndex) override;
	
	virtual void OnLoad_Implementation(EPhase InPhase) override;
	
	virtual void OnUnload_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation() override;
	
protected:
	UPROPERTY()
	FWidgetModuleSaveData SaveData;

	UPROPERTY()
	FWidgetModuleSaveData DefaultData;

public:
	virtual FSaveData* GetSaveData() override { return &SaveData; }

	virtual void SetSaveData(FSaveData* InSaveData) override { SaveData = InSaveData->CastRef<FWidgetModuleSaveData>(); }

	virtual FSaveData* GetDefaultData() override { return &DefaultData; }

	virtual void SetDefaultData(FSaveData* InDefaultData) override { DefaultData = InDefaultData->CastRef<FWidgetModuleSaveData>(); }
};
