// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "EventModuleTypes.h"
#include "Main/Base/ModuleBase.h"

#include "EventModule.generated.h"

class UEventManagerBase;

UCLASS()
class WHFRAMEWORK_API UEventModule : public UModuleBase
{
	GENERATED_BODY()
			
	GENERATED_MODULE(UEventModule)

public:
	// ParamSets default values for this actor's properties
	UEventModule();

	~UEventModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

	//////////////////////////////////////////////////////////////////////////
	/// Event
protected:
	TMap<TSubclassOf<UEventHandleBase>, FEventHandleInfo> EventHandleInfos;

public:
	template<class T>
	void SubscribeEvent(UObject* InOwner, const FName InFuncName)
	{
		SubscribeEvent(T::StaticClass(), InOwner, InFuncName);
	}

	template<class T>
	void SubscribeEvent(const FEventExecuteDynamicDelegate& InDelegate)
	{
		SubscribeEvent(T::StaticClass(), InDelegate);
	}

	UFUNCTION(BlueprintCallable)
	void SubscribeEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InOwner, const FName InFuncName);

	void SubscribeEvent(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDelegate& InDelegate);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Subscribe Event"))
	void SubscribeEventByDelegate(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDynamicDelegate& InDelegate);

	template<class T>
	void UnsubscribeEvent(UObject* InOwner, const FName InFuncName)
	{
		UnsubscribeEvent(T::StaticClass(), InOwner, InFuncName);
	}

	template<class T>
	void UnsubscribeEvent(const FEventExecuteDynamicDelegate& InDelegate)
	{
		UnsubscribeEvent(T::StaticClass(), InDelegate);
	}

	UFUNCTION(BlueprintCallable)
	void UnsubscribeEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InOwner, const FName InFuncName);

	void UnsubscribeEvent(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDelegate& InDelegate);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Unsubscribe Event"))
	void UnsubscribeEventByDelegate(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDynamicDelegate& InDelegate);

	UFUNCTION(BlueprintCallable)
	void UnsubscribeAllEvent();

	template<class T>
	void BroadcastEvent(UObject* InSender, const TArray<FParameter>* InParams = nullptr, EEventNetType InNetType = EEventNetType::Single)
	{
		BroadcastEvent(T::StaticClass(), InSender, InParams ? *InParams : TArray<FParameter>(), InNetType);
	}

	template<class T>
	void BroadcastEvent(UObject* InSender, const TArray<FParameter>& InParams, EEventNetType InNetType = EEventNetType::Single)
	{
		BroadcastEvent(T::StaticClass(), InSender, InParams, InNetType);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	void BroadcastEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InSender, const TArray<FParameter>& InParams, EEventNetType InNetType = EEventNetType::Single);

	UFUNCTION(BlueprintCallable)
	void BroadcastEventByHandle(UEventHandleBase* InHandle, UObject* InSender, EEventNetType InNetType = EEventNetType::Single);

	UFUNCTION(NetMulticast, Reliable)
	void MultiBroadcastEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InSender, const TArray<FParameter>& InParams);

protected:
	UFUNCTION()
	void ExecuteEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InSender, const TArray<FParameter>& InParams);

	//////////////////////////////////////////////////////////////////////////
	/// Event Manager
protected:
	UPROPERTY(EditAnywhere, Category = "EventManager")
	TArray<TSubclassOf<UEventManagerBase>> EventManagers;

	UPROPERTY(Transient)
	TMap<TSubclassOf<UEventManagerBase>, UEventManagerBase*> EventManagerRefs;

public:
	template<class T>
	T* GetEventManager() const
	{
		return Cast<T>(GetEventManager(T::StaticClass()));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UEventManagerBase* GetEventManager(TSubclassOf<UEventManagerBase> InClass) const;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
