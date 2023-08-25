// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Kismet/BlueprintFunctionLibrary.h"

#include "MainModuleBPLibrary.generated.h"

class UModuleNetworkComponentBase;
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
	static void RunModuleByClass(TSubclassOf<AModuleBase> InClass);

	UFUNCTION(BlueprintCallable, Category = "MainModuleBPLibrary")
	static void RunModuleByName(const FName InName);

	UFUNCTION(BlueprintCallable, Category = "MainModuleBPLibrary")
	static void ResetModuleByClass(TSubclassOf<AModuleBase> InClass);

	UFUNCTION(BlueprintCallable, Category = "MainModuleBPLibrary")
	static void ResetModuleByName(const FName InName);

	UFUNCTION(BlueprintCallable, Category = "MainModuleBPLibrary")
	static void PauseModuleByClass(TSubclassOf<AModuleBase> InClass);

	UFUNCTION(BlueprintCallable, Category = "MainModuleBPLibrary")
	static void PauseModuleByName(const FName InName);
		
	UFUNCTION(BlueprintCallable, Category = "MainModuleBPLibrary")
	static void UnPauseModuleByClass(TSubclassOf<AModuleBase> InClass);

	UFUNCTION(BlueprintCallable, Category = "MainModuleBPLibrary")
	static void UnPauseModuleByName(const FName InName);

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
	static TArray<AModuleBase*> GetAllModule(bool bInEditor = false);
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "MainModuleBPLibrary")
	static AModuleBase* GetModuleByClass(TSubclassOf<AModuleBase> InClass, bool bInEditor = false);

	UFUNCTION(BlueprintPure, Category = "MainModuleBPLibrary")
	static AModuleBase* GetModuleByName(const FName InName, bool bInEditor = false);

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "MainModuleBPLibrary")
	static UModuleNetworkComponentBase* GetModuleNetworkComponentByClass(TSubclassOf<UModuleNetworkComponentBase> InModuleNetworkComponentClass, bool bInEditor = false);
};
