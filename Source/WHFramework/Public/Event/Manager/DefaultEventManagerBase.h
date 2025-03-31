// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "EventManagerBase.h"
#include "Common/CommonTypes.h"
#include "Event/EventModuleTypes.h"

#include "DefaultEventManagerBase.generated.h"

class UEventHandle_GameInited;
class UEventHandle_GameExited;
class UEventHandle_GameStarted;

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UDefaultEventManagerBase : public UEventManagerBase
{
	GENERATED_BODY()
	
public:	
	// ParamSets default values for this object's properties
	UDefaultEventManagerBase();

	//////////////////////////////////////////////////////////////////////////
	/// Defaults
public:
	virtual void OnInitialize() override;
	
	virtual void OnPreparatory() override;
	
	virtual void OnRefresh(float DeltaSeconds) override;
	
	virtual void OnTermination(EPhase InPhase) override;
	
	virtual void OnHandleEvent(UObject* InSender, UEventHandleBase* InEventHandle) override;

	//////////////////////////////////////////////////////////////////////////
	/// Events
protected:
	/**
	* 当初始化游戏
	*/
	UFUNCTION(BlueprintNativeEvent)
	void OnGameInited(UObject* InSender, UEventHandle_GameInited* InEventHandle);
	/**
	* 当开始游戏
	*/
	UFUNCTION(BlueprintNativeEvent)
	void OnGameStarted(UObject* InSender, UEventHandle_GameStarted* InEventHandle);
	/**
	* 当退出游戏
	*/
	UFUNCTION(BlueprintNativeEvent)
	void OnGameExited(UObject* InSender, UEventHandle_GameExited* InEventHandle);
};
