// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHObject.h"

#include "EventManagerBase.generated.h"

class UEventHandleBase;
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

	//////////////////////////////////////////////////////////////////////////
	/// Defaults
public:
	/**
	* 当初始化
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnInitialize")
	void K2_OnInitialize();
	UFUNCTION()
	virtual void OnInitialize();
	/**
	* 当准备
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnPreparatory")
	void K2_OnPreparatory(EPhase InPhase);
	UFUNCTION()
	virtual void OnPreparatory(EPhase InPhase);
	/**
	* 当刷新
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnRefresh")
	void K2_OnRefresh(float DeltaSeconds);
	UFUNCTION()
	virtual void OnRefresh(float DeltaSeconds);
	/**
	* 当销毁
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnTermination")
	void K2_OnTermination(EPhase InPhase);
	UFUNCTION()
	virtual void OnTermination(EPhase InPhase);
	/**
	* 当处理事件
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnHandleEvent")
	void K2_OnHandleEvent(UObject* InSender, UEventHandleBase* InEventHandle);
	UFUNCTION()
	virtual void OnHandleEvent(UObject* InSender, UEventHandleBase* InEventHandle);

protected:
	UPROPERTY(EditAnywhere, Category = "Event")
	TArray<TSubclassOf<UEventHandleBase>> EventsToHandle;

	//////////////////////////////////////////////////////////////////////////
	/// Events
protected:
	/**
	* 当初始化游戏
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnInitGame")
	void K2_OnInitGame(UObject* InSender, UEventHandle_InitGame* InEventHandle);
	UFUNCTION()
	virtual void OnInitGame(UObject* InSender, UEventHandle_InitGame* InEventHandle);
	/**
	* 当开始游戏
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnStartGame")
	void K2_OnStartGame(UObject* InSender, UEventHandle_StartGame* InEventHandle);
	UFUNCTION()
	virtual void OnStartGame(UObject* InSender, UEventHandle_StartGame* InEventHandle);
	/**
	* 当退出游戏
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnExitGame")
	void K2_OnExitGame(UObject* InSender, UEventHandle_ExitGame* InEventHandle);
	UFUNCTION()
	virtual void OnExitGame(UObject* InSender, UEventHandle_ExitGame* InEventHandle);
};
