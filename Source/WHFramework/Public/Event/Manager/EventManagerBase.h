// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/CommonTypes.h"
#include "Common/Base/WHObject.h"
#include "Event/EventModuleTypes.h"

#include "EventManagerBase.generated.h"

class UEventHandleBase;
class UEventHandle_GameInited;
class UEventHandle_GameExited;
class UEventHandle_GameStarted;

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
	void K2_OnPreparatory();
	UFUNCTION()
	virtual void OnPreparatory();
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
	FName EventManagerName;

	UPROPERTY(EditAnywhere, Category = "Event")
	TArray<TSubclassOf<UEventHandleBase>> EventHandleClasses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (TitleProperty = "Tag"), Category = "Event")
	TArray<FEventInfo> EventInfos;

public:
	UFUNCTION(BlueprintPure)
	FName GetEventManagerName() const { return EventManagerName; }

	UFUNCTION(BlueprintPure)
	TArray<TSubclassOf<UEventHandleBase>> GetEventHandleClasses() const { return EventHandleClasses; }

	UFUNCTION(BlueprintPure)
	bool GetEventInfoByTag(const FGameplayTag& InTag, FEventInfo& OutMenuInfo) const;

	//////////////////////////////////////////////////////////////////////////
	/// Events
protected:
	/**
	* 当初始化游戏
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnGameInited")
	void K2_OnGameInited(UObject* InSender, UEventHandle_GameInited* InEventHandle);
	UFUNCTION()
	virtual void OnGameInited(UObject* InSender, UEventHandle_GameInited* InEventHandle);
	/**
	* 当开始游戏
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnGameStarted")
	void K2_OnGameStarted(UObject* InSender, UEventHandle_GameStarted* InEventHandle);
	UFUNCTION()
	virtual void OnGameStarted(UObject* InSender, UEventHandle_GameStarted* InEventHandle);
	/**
	* 当退出游戏
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnGameExited")
	void K2_OnGameExited(UObject* InSender, UEventHandle_GameExited* InEventHandle);
	UFUNCTION()
	virtual void OnGameExited(UObject* InSender, UEventHandle_GameExited* InEventHandle);
};
