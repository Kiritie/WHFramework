// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"

#include "GameFramework/Actor.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "ReferencePool/ReferencePoolInterface.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "WHActor.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UWHActorInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORK_API IWHActorInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	/**
	* 当初始化
	*/
	UFUNCTION(BlueprintNativeEvent)
	void OnInitialize();
	/**
	* 当准备
	*/
	UFUNCTION(BlueprintNativeEvent)
	void OnPreparatory(EPhase InPhase);
	/**
	* 当刷新
	*/
	UFUNCTION(BlueprintNativeEvent)
	void OnRefresh(float DeltaSeconds);
	/**
	* 当结束
	*/
	UFUNCTION(BlueprintNativeEvent)
	void OnTermination();

protected:
	/**
	* 是否使用默认生命周期
	*/
	UFUNCTION(BlueprintNativeEvent)
	bool IsDefaultLifecycle() const;
};

/**
 * 
 */
UCLASS(hidecategories = (Tick, Replication, Rendering, Collision, Actor, Input, LOD, Cooking, Hidden, WorldPartition, Hlod))
class WHFRAMEWORK_API AWHActor : public AActor, public IWHActorInterface, public ISceneActorInterface, public IObjectPoolInterface, public IReferencePoolInterface
{
	GENERATED_BODY()
	
public:	
	AWHActor();

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
	/// ObjectPool
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual void OnReset_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	/// SceneActor
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actor")
	FGuid ActorID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actor")
	bool bVisible;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Actor")
	TScriptInterface<ISceneContainerInterface> Container;
	
public:
	virtual FGuid GetActorID_Implementation() const override { return ActorID; }

	virtual void SetActorID_Implementation(FGuid InActorID) override { ActorID = InActorID; }

	virtual TScriptInterface<ISceneContainerInterface> GetContainer_Implementation() const override { return Container; }

	virtual void SetContainer_Implementation(const TScriptInterface<ISceneContainerInterface>& InContainer) override { Container = InContainer; }

	virtual bool IsVisible_Implementation() const override { return bVisible; }

	virtual void SetActorVisible_Implementation(bool bNewVisible) override;

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaSeconds) override;
};
