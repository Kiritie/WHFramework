// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHObject.h"

#include "EventManagerBase.generated.h"

class UEventHandle_EndPlay;
class UEventHandle_BeginPlay;

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
	void OnBeginPlay(UObject* InSender, UEventHandle_BeginPlay* InEventHandle);

	UFUNCTION(BlueprintNativeEvent)
	void OnEndPlay(UObject* InSender, UEventHandle_EndPlay* InEventHandle);
};
