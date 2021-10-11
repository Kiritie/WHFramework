// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "ProcedureModuleBPLibrary.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UProcedureModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "ProcedureModuleBPLibrary")
	static class AProcedureBase* GetCurrentProcedure();
	
	UFUNCTION(BlueprintPure, Category = "ProcedureModuleBPLibrary")
	static class AProcedureBase* GetCurrentLocalProcedure();
	
	UFUNCTION(BlueprintPure, Category = "ProcedureModuleBPLibrary")
	static class AProcedureBase* GetCurrentParentProcedure();

	UFUNCTION(BlueprintPure, Category = "ProcedureModuleBPLibrary")
	static class ARootProcedureBase* GetCurrentRootProcedure();

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void ServerEnterProcedure(class AProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void ServerLeaveProcedure(class AProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void LocalEnterProcedure(class AProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void LocalLeaveProcedure(class AProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void ServerSkipCurrentProcedure();

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void ServerSkipCurrentParentProcedure();

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void ServerSkipCurrentRootProcedure();
};
