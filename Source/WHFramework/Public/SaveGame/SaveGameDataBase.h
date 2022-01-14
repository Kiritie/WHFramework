// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "SaveGameDataBase.generated.h"

/**
* 数据存取数据基类
*/
UCLASS(DefaultToInstanced, Blueprintable)
class WHFRAMEWORK_API USaveGameDataBase : public UObject
{
	GENERATED_BODY()

public:
	USaveGameDataBase();

public:
	UFUNCTION(BlueprintNativeEvent)
	void OnInitialize();
};
