// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/SaveGame.h"
#include "SaveGameBase.generated.h"

/**
* 数据存取基类
*/
UCLASS()
class WHFRAMEWORK_API USaveGameBase : public USaveGame
{
	GENERATED_BODY()

public:
	USaveGameBase();

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void RefreshSaveData();
};
