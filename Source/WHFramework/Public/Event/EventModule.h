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
	/// ModuleBase
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds, bool bInEditor) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

	//////////////////////////////////////////////////////////////////////////
	/// Event
protected:
	UPROPERTY(VisibleAnywhere)
	TMap<TSubclassOf<UEventHandleBase>, FEventMapping> EventMappings;

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

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Unsubscribe Event"))
	void UnsubscribeEventByDelegate(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDynamicDelegate& InDelegate);

	UFUNCTION(BlueprintCallable)
	void UnsubscribeAllEvent();

	template<class T>
	void BroadcastEvent(UObject* InSender, const TArray<FParameter>* InParams = nullptr, EEventNetType InNetType = EEventNetType::Single, bool bRecovery = true)
	{
		BroadcastEvent(T::StaticClass(), InSender, InParams ? *InParams : TArray<FParameter>(), InNetType, bRecovery);
	}

	template<class T>
	void BroadcastEvent(UObject* InSender, const TArray<FParameter>& InParams, EEventNetType InNetType = EEventNetType::Single, bool bRecovery = true)
	{
		BroadcastEvent(T::StaticClass(), InSender, InParams, InNetType, bRecovery);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	void BroadcastEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InSender, const TArray<FParameter>& InParams, EEventNetType InNetType = EEventNetType::Single, bool bRecovery = true);

	UFUNCTION(BlueprintCallable)
	void BroadcastEventByHandle(UEventHandleBase* InHandle, UObject* InSender, EEventNetType InNetType = EEventNetType::Single, bool bRecovery = true);

	UFUNCTION(NetMulticast, Reliable)
	void MultiBroadcastEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InSender, const TArray<FParameter>& InParams, bool bRecovery = true);

protected:
	UFUNCTION()
	void ExecuteEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InSender, const TArray<FParameter>& InParams, bool bRecovery = true);

	//////////////////////////////////////////////////////////////////////////
	/// Event Manager
protected:
	UPROPERTY(EditAnywhere, Instanced)
	TArray<UEventManagerBase*> EventManagers;

	UPROPERTY(Transient)
	TMap<FName, UEventManagerBase*> EventManagerRefs;

public:
	template<class T>
	T* GetEventManager() const
	{
		return Cast<T>(GetEventManager(T::StaticClass()));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UEventManagerBase* GetEventManager(TSubclassOf<UEventManagerBase> InClass) const;

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UEventManagerBase* GetEventManagerByName(const FName InName, TSubclassOf<UEventManagerBase> InClass = nullptr) const;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
