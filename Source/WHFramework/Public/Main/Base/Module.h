// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Global/Base/WHActor.h"
#include "UObject/Interface.h"
#include "Module.generated.h"

UENUM(BlueprintType)
enum class EModuleState : uint8
{
	None,
	Running,
	Paused,
	Terminated
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FModuleStateChanged, EModuleState, InModuleState);

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UModule : public UWHActorInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORK_API IModule : public IWHActorInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
// #if WITH_EDITOR
	/**
	* 当构建
	*/
	UFUNCTION(BlueprintNativeEvent)
	void OnGenerate();
	/**
	* 当销毁
	*/
	UFUNCTION(BlueprintNativeEvent)
	void OnDestroy();
// #endif
	/**
	* 当状态改变
	*/
	UFUNCTION(BlueprintNativeEvent)
	void OnStateChanged(EModuleState InModuleState);
	/**
	* 当暂停
	*/
	UFUNCTION(BlueprintNativeEvent)
	void OnPause();
	/**
	* 当恢复
	*/
	UFUNCTION(BlueprintNativeEvent)
	void OnUnPause();

public:
	/**
	* 运行
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Run();
	/**
	* 暂停
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Pause();
	/**
	* 恢复
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void UnPause();
	/**
	* 结束
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Termination();
	
public:
	/**
	* 是否自动运行
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	bool IsAutoRunModule() const;
	/**
	* 获取模块名称
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	FName GetModuleName() const;
	/**
	* 获取模块状态
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	EModuleState GetModuleState() const;
};
