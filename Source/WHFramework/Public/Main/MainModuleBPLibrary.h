// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "MainModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "MainModuleBPLibrary.generated.h"

class IModule;
class AModuleBase;
class AMainModule;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UMainModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static AMainModule* MainModuleInst;

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "MainModuleBPLibrary")
	static AMainModule* GetMainModule(const UObject* InWorldContext);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext", DisplayName = "Get Module By Class", DeterminesOutputType = "InModuleClass"), Category = "MainModuleBPLibrary")
	static AModuleBase* K2_GetModuleByClass(const UObject* InWorldContext, TSubclassOf<AModuleBase> InModuleClass);
	
	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext", DisplayName = "Get Module By Name"), Category = "MainModuleBPLibrary")
	static TScriptInterface<IModule> K2_GetModuleByName(const UObject* InWorldContext, const FName InModuleName);
};
