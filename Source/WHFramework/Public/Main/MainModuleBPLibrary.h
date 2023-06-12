// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "MainModuleBPLibrary.generated.h"

class UModuleNetworkComponent;
class IModule;
class AMainModule;
class AModuleBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UMainModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MainModuleBPLibrary")
	static void RunModuleByClass(TSubclassOf<AModuleBase> InModuleClass);

	UFUNCTION(BlueprintCallable, Category = "MainModuleBPLibrary")
	static void RunModuleByName(const FName InModuleName);

	UFUNCTION(BlueprintCallable, Category = "MainModuleBPLibrary")
	static void PauseModuleByClass(TSubclassOf<AModuleBase> InModuleClass);

	UFUNCTION(BlueprintCallable, Category = "MainModuleBPLibrary")
	static void PauseModuleByName(const FName InModuleName);
		
	UFUNCTION(BlueprintCallable, Category = "MainModuleBPLibrary")
	static void UnPauseModuleByClass(TSubclassOf<AModuleBase> InModuleClass);

	UFUNCTION(BlueprintCallable, Category = "MainModuleBPLibrary")
	static void UnPauseModuleByName(const FName InModuleName);

	UFUNCTION(BlueprintCallable, Category = "MainModuleBPLibrary")
	static void PauseMainModule();

	UFUNCTION(BlueprintCallable, Category = "MainModuleBPLibrary")
	static void UnPauseMainModule();

	UFUNCTION(BlueprintCallable, Category = "MainModuleBPLibrary")
	static void PauseAllModule();

	UFUNCTION(BlueprintCallable, Category = "MainModuleBPLibrary")
	static void UnPauseAllModule();
				
public:
	UFUNCTION(BlueprintPure, Category = "MainModuleBPLibrary")
	static AMainModule* GetMainModule(bool bInEditor = false);

	UFUNCTION(BlueprintPure, Category = "MainModuleBPLibrary")
	static TArray<TScriptInterface<IModule>> GetAllModule(bool bInEditor = false);
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InModuleClass"), Category = "MainModuleBPLibrary")
	static AModuleBase* GetModuleByClass(TSubclassOf<AModuleBase> InModuleClass, bool bInEditor = false);

	UFUNCTION(BlueprintPure, Category = "MainModuleBPLibrary")
	static TScriptInterface<IModule> GetModuleByName(const FName InModuleName, bool bInEditor = false);

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InModuleClass"), Category = "MainModuleBPLibrary")
	static UModuleNetworkComponent* GetModuleNetworkComponentByClass(TSubclassOf<UModuleNetworkComponent> InModuleNetworkComponentClass, bool bInEditor = false);
};
