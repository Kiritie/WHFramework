// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Kismet/BlueprintFunctionLibrary.h"

#include "MainModuleStatics.generated.h"

class UModuleNetworkComponentBase;
class AMainModule;
class UModuleBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UMainModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MainModuleStatics")
	static void RunModuleByClass(TSubclassOf<UModuleBase> InClass);

	UFUNCTION(BlueprintCallable, Category = "MainModuleStatics")
	static void RunModuleByName(const FName InName);

	UFUNCTION(BlueprintCallable, Category = "MainModuleStatics")
	static void ResetModuleByClass(TSubclassOf<UModuleBase> InClass);

	UFUNCTION(BlueprintCallable, Category = "MainModuleStatics")
	static void ResetModuleByName(const FName InName);

	UFUNCTION(BlueprintCallable, Category = "MainModuleStatics")
	static void PauseModuleByClass(TSubclassOf<UModuleBase> InClass);

	UFUNCTION(BlueprintCallable, Category = "MainModuleStatics")
	static void PauseModuleByName(const FName InName);
		
	UFUNCTION(BlueprintCallable, Category = "MainModuleStatics")
	static void UnPauseModuleByClass(TSubclassOf<UModuleBase> InClass);

	UFUNCTION(BlueprintCallable, Category = "MainModuleStatics")
	static void UnPauseModuleByName(const FName InName);

	UFUNCTION(BlueprintCallable, Category = "MainModuleStatics")
	static void PauseAllModule();

	UFUNCTION(BlueprintCallable, Category = "MainModuleStatics")
	static void UnPauseAllModule();
				
	UFUNCTION(BlueprintCallable, Category = "MainModuleStatics")
	static void TerminationModuleByClass(TSubclassOf<UModuleBase> InClass);

	UFUNCTION(BlueprintCallable, Category = "MainModuleStatics")
	static void TerminationModuleByName(const FName InName);

public:
	UFUNCTION(BlueprintPure, Category = "MainModuleStatics")
	static AMainModule* GetMainModule(bool bInEditor = false);

	UFUNCTION(BlueprintPure, Category = "MainModuleStatics")
	static TArray<UModuleBase*> GetAllModule(bool bInEditor = false);
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "MainModuleStatics")
	static UModuleBase* GetModuleByClass(TSubclassOf<UModuleBase> InClass, bool bInEditor = false);

	UFUNCTION(BlueprintPure, Category = "MainModuleStatics")
	static UModuleBase* GetModuleByName(const FName InName, bool bInEditor = false);

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "MainModuleStatics")
	static UModuleNetworkComponentBase* GetModuleNetworkComponentByClass(TSubclassOf<UModuleNetworkComponentBase> InClass);
};
