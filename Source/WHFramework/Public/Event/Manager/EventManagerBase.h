// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHObject.h"

#include "EventManagerBase.generated.h"

class UEventHandle_InitGame;
class UEventHandle_ExitGame;
class UEventHandle_StartGame;

UCLASS()
class WHFRAMEWORK_API UEventManagerBase : public UWHObject
{
	GENERATED_BODY()
	
public:	
	// ParamSets default values for this object's properties
	UEventManagerBase();

public:
	UFUNCTION(BlueprintNativeEvent)
	void OnInitialize();

protected:
	UFUNCTION(BlueprintNativeEvent)
	void OnInitGame(UObject* InSender, UEventHandle_InitGame* InEventHandle);

	UFUNCTION(BlueprintNativeEvent)
	void OnStartGame(UObject* InSender, UEventHandle_StartGame* InEventHandle);

	UFUNCTION(BlueprintNativeEvent)
	void OnExitGame(UObject* InSender, UEventHandle_ExitGame* InEventHandle);
};
