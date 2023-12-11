// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "EventModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EventModuleStatics.generated.h"

class UEventManagerBase;
class UEventHandleBase;
class UEventModule;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UEventModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	template<class T>
	static void SubscribeEvent(UObject* InOwner, const FName InFuncName)
	{
		SubscribeEvent(T::StaticClass(), InOwner, InFuncName);
	}
	template<class T>
	static void SubscribeEvent(const FEventExecuteDynamicDelegate& InDelegate)
	{
		SubscribeEvent(T::StaticClass(), InDelegate);
	}

	UFUNCTION(BlueprintCallable, Category = "EventModuleStatics")
	static void SubscribeEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InOwner, const FName InFuncName);

	static void SubscribeEvent(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDelegate& InDelegate);

	UFUNCTION(BlueprintCallable, Category = "EventModuleStatics", meta = (DisplayName = "Subscribe Event"))
	static void SubscribeEventByDelegate(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDynamicDelegate& InDelegate);

	template<class T>
	static void UnsubscribeEvent(UObject* InOwner, const FName InFuncName)
	{
		UnsubscribeEvent(T::StaticClass(), InOwner, InFuncName);
	}

	template<class T>
	static void UnsubscribeEvent(const FEventExecuteDynamicDelegate& InDelegate)
	{
		UnsubscribeEvent(T::StaticClass(), InDelegate);
	}

	UFUNCTION(BlueprintCallable, Category = "EventModuleStatics")
	static void UnsubscribeEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InOwner, const FName InFuncName);

	static void UnsubscribeEvent(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDelegate& InDelegate);

	UFUNCTION(BlueprintCallable, Category = "EventModuleStatics", meta = (DisplayName = "Unsubscribe Event"))
	static void UnsubscribeEventByDelegate(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDynamicDelegate& InDelegate);

	UFUNCTION(BlueprintCallable, Category = "EventModuleStatics")
	static void UnsubscribeAllEvent();

	template<class T>
	static void BroadcastEvent(UObject* InSender, const TArray<FParameter>* InParams = nullptr, EEventNetType InNetType = EEventNetType::Single, bool bRecovery = true)
	{
		BroadcastEvent(T::StaticClass(), InSender, InParams ? *InParams : TArray<FParameter>(), InNetType, bRecovery);
	}

	template<class T>
	static void BroadcastEvent(UObject* InSender, const TArray<FParameter>& InParams, EEventNetType InNetType = EEventNetType::Single, bool bRecovery = true)
	{
		BroadcastEvent(T::StaticClass(), InSender, InParams, InNetType, bRecovery);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"), Category = "EventModuleStatics")
	static void BroadcastEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InSender, const TArray<FParameter>& InParams, EEventNetType InNetType = EEventNetType::Single, bool bRecovery = true);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Broadcast Event"), Category = "EventModuleStatics")
	static void BroadcastEventByHandle(UEventHandleBase* InHandle, UObject* InSender, EEventNetType InNetType = EEventNetType::Single, bool bRecovery = true);

public:
	template<class T>
	static T* GetEventManager()
	{
		return Cast<T>(GetEventManager(T::StaticClass()));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "EventModuleStatics")
	static UEventManagerBase* GetEventManager(TSubclassOf<UEventManagerBase> InClass);
};
