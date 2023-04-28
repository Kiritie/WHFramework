// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"

#include "Main/Base/Module.h"
#include "Global/Base/WHActor.h"
#include "Main/MainModuleTypes.h"
#include "ModuleBase.generated.h"

UCLASS(hidecategories = (Rendering, Collision, Physics))
class WHFRAMEWORK_API AModuleBase : public AWHActor, public IModule
{
	GENERATED_BODY()
	
public:	
	// ParamSets default values for this actor's properties
	AModuleBase();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate_Implementation() override;

	virtual void OnDestroy_Implementation() override;
#endif

	virtual void OnStateChanged_Implementation(EModuleState InModuleState) override;

	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	virtual void OnTermination_Implementation() override;

protected:
	virtual bool IsDefaultLifecycle_Implementation() const override { return false; }

public:
	virtual void Run_Implementation() override;
	
	virtual void Pause_Implementation() override;

	virtual void UnPause_Implementation() override;

	virtual void Termination_Implementation() override;

protected:
	/// 自动运行
	UPROPERTY(EditAnywhere, Replicated)
	bool bAutoRunModule;
	/// 模块名称
	UPROPERTY(EditDefaultsOnly, Replicated)
	FName ModuleName;
	/// 模块状态
	UPROPERTY(VisibleAnywhere, Replicated)
	EModuleState ModuleState;

public:
	UPROPERTY(BlueprintAssignable)
	FModuleStateChanged OnModuleStateChanged;

public:
	virtual bool IsAutoRunModule_Implementation() const override { return bAutoRunModule; }

	virtual FName GetModuleName_Implementation() const override { return ModuleName; }

	virtual EModuleState GetModuleState_Implementation() const override { return ModuleState; }

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
