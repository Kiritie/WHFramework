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
	static AEventModule* EventModuleInst;

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "EventModuleBPLibrary")
	static AEventModule* GetEventModule(const UObject* InWorldContext);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "EventModuleBPLibrary")
	static class UEventModuleNetworkComponent* GetEventModuleNetworkComponent(UObject* InWorldContext);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "EventModuleBPLibrary")
	static void SubscribeEvent(const UObject* InWorldContext, TSubclassOf<UEventHandleBase> InEventHandleClass, UObject* InOwner, const FName InFuncName);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "EventModuleBPLibrary")
	static void UnsubscribeEvent(const UObject* InWorldContext, TSubclassOf<UEventHandleBase> InEventHandleClass, UObject* InOwner, const FName InFuncName);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "EventModuleBPLibrary")
	static void UnsubscribeAllEvent(const UObject* InWorldContext);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "EventModuleBPLibrary")
	static void BroadcastEvent(const UObject* InWorldContext, TSubclassOf<UEventHandleBase> InEventHandleClass, EEventNetType InEventNetType, UObject* InSender, const TArray<FParameter>& InParameters);
};
