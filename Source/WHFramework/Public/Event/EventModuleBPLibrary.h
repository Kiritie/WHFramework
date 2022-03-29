// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

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
	UFUNCTION(BlueprintCallable, Category = "EventModuleBPLibrary")
	static void SubscribeEvent(TSubclassOf<UEventHandleBase> InEventHandleClass, UObject* InOwner, const FName InFuncName);

	UFUNCTION(BlueprintCallable, Category = "EventModuleBPLibrary")
	static void UnsubscribeEvent(TSubclassOf<UEventHandleBase> InEventHandleClass, UObject* InOwner, const FName InFuncName);

	UFUNCTION(BlueprintCallable, Category = "EventModuleBPLibrary")
	static void UnsubscribeAllEvent();

	static void BroadcastEvent(TSubclassOf<UEventHandleBase> InEventHandleClass, EEventNetType InEventNetType, UObject* InSender, const TArray<FParameter>* InParameters = nullptr);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParameters"), Category = "EventModuleBPLibrary")
	static void BroadcastEvent(TSubclassOf<UEventHandleBase> InEventHandleClass, EEventNetType InEventNetType, UObject* InSender, const TArray<FParameter>& InParameters);
};
