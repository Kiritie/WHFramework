// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Global/Base/WHActor.h"
#include "Main/MainModuleTypes.h"
#include "ModuleBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FModuleStateChanged, EModuleState, InModuleState);

UCLASS(hidecategories = (Rendering, Collision, Physics))
class WHFRAMEWORK_API AModuleBase : public AWHActor
{
	GENERATED_BODY()
	
public:	
	// ParamSets default values for this actor's properties
	AModuleBase();
	
	//////////////////////////////////////////////////////////////////////////
	/// WHActor
public:
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnTermination_Implementation() override;

protected:
	virtual bool IsDefaultLifecycle_Implementation() const override { return false; }

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	/**
	* 当构建
	*/
	virtual void OnGenerate();
	/**
	* 当销毁
	*/
	virtual void OnDestroy();
#endif
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
	UFUNCTION(BlueprintNativeEvent)
	void Run();
	/**
	* 暂停
	*/
	UFUNCTION(BlueprintNativeEvent)
	void Pause();
	/**
	* 恢复
	*/
	UFUNCTION(BlueprintNativeEvent)
	void UnPause();
	/**
	* 结束
	*/
	UFUNCTION(BlueprintNativeEvent)
	void Termination();

protected:
	/// 自动运行
	UPROPERTY(EditAnywhere)
	bool bAutoRunModule;
	/// 模块名称
	UPROPERTY(EditDefaultsOnly)
	FName ModuleName;
	/// 模块状态
	UPROPERTY(VisibleAnywhere, Replicated)
	EModuleState ModuleState;

public:
	UPROPERTY(BlueprintAssignable)
	FModuleStateChanged OnModuleStateChanged;

public:
	/**
	* 是否自动运行
	*/
	UFUNCTION(BlueprintPure)
	bool IsAutoRunModule() const;
	/**
	* 获取模块名称
	*/
	UFUNCTION(BlueprintPure)
	FName GetModuleName() const;
	/**
	* 获取模块状态
	*/
	UFUNCTION(BlueprintPure)
	EModuleState GetModuleState() const;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
