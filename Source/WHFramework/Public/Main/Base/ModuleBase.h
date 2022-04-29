// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Main/Base/Module.h"
#include "GameFramework/Actor.h"
#include "ModuleBase.generated.h"

UCLASS(hidecategories = (Tick, Replication, Rendering, Collision, Actor, Input, LOD, Cooking, Hidden, WorldPartition, Hlod))
class WHFRAMEWORK_API AModuleBase : public AActor, public IModule
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

	virtual void OnPreparatory_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	virtual void OnTermination_Implementation() override;

public:
	virtual void Pause_Implementation() override;

	virtual void UnPause_Implementation() override;

protected:
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
	virtual FName GetModuleName_Implementation() const override { return ModuleName; }

	virtual EModuleState GetModuleState_Implementation() const override { return ModuleState; }

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
