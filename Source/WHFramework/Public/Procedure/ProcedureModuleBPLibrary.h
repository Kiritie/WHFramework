// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProcedureModuleTypes.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "ProcedureModuleBPLibrary.generated.h"

class URootProcedureBase;
class UProcedureBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UProcedureModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "ProcedureModuleBPLibrary")
	static UProcedureBase* GetCurrentProcedure();

	UFUNCTION(BlueprintPure, Category = "ProcedureModuleBPLibrary")
	static URootProcedureBase* GetCurrentRootProcedure();

public:
	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void StartProcedure(int32 InRootProcedureIndex = -1, bool bSkipProcedures = false);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void EndProcedure(bool bRestoreProcedures = false);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void RestoreProcedure(UProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void EnterProcedure(UProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void RefreshProcedure(UProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void GuideProcedure(UProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void ExecuteProcedure(UProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void CompleteProcedure(UProcedureBase* InProcedure, EProcedureExecuteResult InProcedureExecuteResult = EProcedureExecuteResult::Succeed);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void LeaveProcedure(UProcedureBase* InProcedure);
};
