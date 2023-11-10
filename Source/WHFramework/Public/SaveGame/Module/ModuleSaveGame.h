// Fill out your copyright notice in the Description page of Project Modules.

#pragma once

#include "SaveGame/Base/SaveGameBase.h"

#include "ModuleSaveGame.generated.h"

class UModuleBase;
class UArchiveSaveGame;

/**
 * 常模块数据存取类
 */
UCLASS()
class WHFRAMEWORK_API UModuleSaveGame : public USaveGameBase
{
	GENERATED_BODY()

public:
	UModuleSaveGame();
	
public:
	virtual void OnCreate_Implementation(int32 InIndex) override;
	
	virtual void OnLoad_Implementation(EPhase InPhase) override;
	
	virtual void OnUnload_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation() override;

protected:
	UPROPERTY(EditAnywhere)
	TSubclassOf<UModuleBase> ModuleClass;
};
