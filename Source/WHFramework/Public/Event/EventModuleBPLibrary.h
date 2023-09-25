// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "EventModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EventModuleBPLibrary.generated.h"

class UEventHandleBase;
class AEventModule;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UEventModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	template<class T>
	static void SubscribeEvent(UObject* InOwner, const FName InFuncName)
	{
		SubscribeEvent(T::StaticClass(), InOwner, InFuncName);
	}
	template<class T>
	static void SubscribeEvent(const FEventExecuteDelegate& InDelegate)
	{
		SubscribeEvent(T::StaticClass(), InDelegate);
	}

	UFUNCTION(BlueprintCallable, Category = "EventModuleBPLibrary")
	static void SubscribeEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InOwner, const FName InFuncName);

	static void SubscribeEvent(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDelegate& InDelegate);

	UFUNCTION(BlueprintCallable, Category = "EventModuleBPLibrary", meta = (DisplayName = "Subscribe Event"))
	static void SubscribeEventByDelegate(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDelegate& InDelegate);

	template<class T>
	static void UnsubscribeEvent(UObject* InOwner, const FName InFuncName)
	{
		UnsubscribeEvent(T::StaticClass(), InOwner, InFuncName);
	}

	template<class T>
	static void UnsubscribeEvent(const FEventExecuteDelegate& InDelegate)
	{
		UnsubscribeEvent(T::StaticClass(), InDelegate);
	}

	UFUNCTION(BlueprintCallable, Category = "EventModuleBPLibrary")
	static void UnsubscribeEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InOwner, const FName InFuncName);

	static void UnsubscribeEvent(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDelegate& InDelegate);

	UFUNCTION(BlueprintCallable, Category = "EventModuleBPLibrary", meta = (DisplayName = "Unsubscribe Event"))
	static void UnsubscribeEventByDelegate(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDelegate& InDelegate);

	UFUNCTION(BlueprintCallable, Category = "EventModuleBPLibrary")
	static void UnsubscribeAllEvent();

	template<class T>
	static void BroadcastEvent(EEventNetType InNetType, UObject* InSender, const TArray<FParameter>* InParams = nullptr)
	{
		BroadcastEvent(T::StaticClass(), InNetType, InSender, InParams ? *InParams : TArray<FParameter>());
	}

	template<class T>
	static void BroadcastEvent(EEventNetType InNetType, UObject* InSender, const TArray<FParameter>& InParams)
	{
		BroadcastEvent(T::StaticClass(), InNetType, InSender, InParams);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"), Category = "EventModuleBPLibrary")
	static void BroadcastEvent(TSubclassOf<UEventHandleBase> InClass, EEventNetType InNetType, UObject* InSender, const TArray<FParameter>& InParams);
};
