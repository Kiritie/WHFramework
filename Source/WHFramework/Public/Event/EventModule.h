// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"

#include "EventModuleTypes.h"
#include "Main/Base/ModuleBase.h"

#include "EventModule.generated.h"

UCLASS()
class WHFRAMEWORK_API AEventModule : public AModuleBase
{
	GENERATED_BODY()
			
	MODULE_INSTANCE_DECLARE(AEventModule)

public:
	// ParamSets default values for this actor's properties
	AEventModule();

protected:
	TMap<TSubclassOf<UEventHandleBase>, FEventHandleInfo> EventHandleInfos;

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate_Implementation() override;

	virtual void OnDestroy_Implementation() override;
#endif

	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	/// Event
public:
	template<class T>
	void SubscribeEvent(UObject* InOwner, const FName InFuncName)
	{
		SubscribeEvent(T::StaticClass(), InOwner, InFuncName);
	}

	UFUNCTION(BlueprintCallable)
	void SubscribeEvent(TSubclassOf<UEventHandleBase> InEventHandleClass, UObject* InOwner, const FName InFuncName);

	template<class T>
	void UnsubscribeEvent(UObject* InOwner, const FName InFuncName)
	{
		UnsubscribeEvent(T::StaticClass(), InOwner, InFuncName);
	}

	UFUNCTION(BlueprintCallable)
	void UnsubscribeEvent(TSubclassOf<UEventHandleBase> InEventHandleClass, UObject* InOwner, const FName InFuncName);

	UFUNCTION(BlueprintCallable)
	void UnsubscribeAllEvent();

	template<class T>
	void BroadcastEvent(EEventNetType InEventNetType, UObject* InSender, const TArray<FParameter>* InParams = nullptr)
	{
		BroadcastEvent(T::StaticClass(), InEventNetType, InSender, InParams ? *InParams : TArray<FParameter>());
	}

	template<class T>
	void BroadcastEvent(EEventNetType InEventNetType, UObject* InSender, const TArray<FParameter>& InParams)
	{
		BroadcastEvent(T::StaticClass(), InEventNetType, InSender, InParams);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	void BroadcastEvent(TSubclassOf<UEventHandleBase> InEventHandleClass, EEventNetType InEventNetType, UObject* InSender, const TArray<FParameter>& InParams);

	UFUNCTION(NetMulticast, Reliable)
	void MultiBroadcastEvent(TSubclassOf<UEventHandleBase> InEventHandleClass, UObject* InSender, const TArray<FParameter>& InParams);

protected:
	UFUNCTION()
	void ExecuteEvent(TSubclassOf<UEventHandleBase> InEventHandleClass, UObject* InSender, const TArray<FParameter>& InParams);

	//////////////////////////////////////////////////////////////////////////
	/// Event Manager
protected:
	UPROPERTY(EditAnywhere, Category = "EventManager")
	TSubclassOf<class AEventManagerBase> EventManagerClass;

	UPROPERTY(VisibleAnywhere, Replicated, Category = "EventManager")
	class AEventManagerBase* EventManager;

public:
	void SpawnEventManager();

	void DestroyEventManager();

	UFUNCTION(BlueprintPure)
	class AEventManagerBase* GetEventManager() const { return EventManager; }

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
