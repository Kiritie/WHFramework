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

	UFUNCTION(BlueprintCallable, Category = "EventModuleBPLibrary")
	static void SubscribeEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InOwner, const FName InFuncName);

	template<class T>
	static void UnsubscribeEvent(UObject* InOwner, const FName InFuncName)
	{
		UnsubscribeEvent(T::StaticClass(), InOwner, InFuncName);
	}

	UFUNCTION(BlueprintCallable, Category = "EventModuleBPLibrary")
	static void UnsubscribeEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InOwner, const FName InFuncName);

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
