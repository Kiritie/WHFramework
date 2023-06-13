// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "Global/Base/WHActor.h"

#include "EventManagerBase.generated.h"

class UEventHandle_EndPlay;
class UEventHandle_BeginPlay;

UCLASS()
class WHFRAMEWORK_API AEventManagerBase : public AWHActor
{
	GENERATED_BODY()
	
public:	
	// ParamSets default values for this actor's properties
	AEventManagerBase();

public:
	virtual void OnInitialize_Implementation() override;

protected:
	UFUNCTION(BlueprintNativeEvent)
	void OnBeginPlay(UObject* InSender, UEventHandle_BeginPlay* InEventHandle);

	UFUNCTION(BlueprintNativeEvent)
	void OnEndPlay(UObject* InSender, UEventHandle_EndPlay* InEventHandle);
};
