// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProcedureModule.h"
#include "ProcedureModuleTypes.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Main/MainModule.h"

#include "ProcedureModuleBPLibrary.generated.h"

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
	static bool HasProcedureByIndex(int32 InProcedureIndex);

	template<class T>
	static T* GetProcedureByIndex(int32 InProcedureIndex)
	{
		if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
		{
			return ProcedureModule->GetProcedureByIndex<T>(InProcedureIndex);
		}
		return nullptr;
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetProcedureByIndex", DeterminesOutputType = "InProcedureIndex"))
	static UProcedureBase* K2_GetProcedureByIndex(int32 InProcedureIndex, TSubclassOf<UProcedureBase> InProcedureClass);
	
	template<class T>
	static bool HasProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass = T::StaticClass())
	{
		if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
		{
			return ProcedureModule->HasProcedureByClass<T>(InProcedureClass);
		}
		return nullptr;
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "HasProcedureByClass"))
	static bool K2_HasProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass);

	template<class T>
	static T* GetProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass = T::StaticClass())
	{
		if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
		{
			return ProcedureModule->GetProcedureByClass<T>(InProcedureClass);
		}
		return nullptr;
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetProcedureByClass", DeterminesOutputType = "InProcedureClass"))
	static UProcedureBase* K2_GetProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass);

public:
	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void StartProcedure(int32 InProcedureIndex = -1, bool bSkipProcedures = false);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void EndProcedure(bool bRestoreProcedures = false);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void RestoreProcedure(UProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void RestoreProcedureByIndex(int32 InProcedureIndex);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void RestoreProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void EnterProcedure(UProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void EnterProcedureByIndex(int32 InProcedureIndex);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void EnterProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void RefreshProcedure(UProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void RefreshProcedureByIndex(int32 InProcedureIndex);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void RefreshProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void GuideProcedure(UProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void GuideProcedureByIndex(int32 InProcedureIndex);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void GuideProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void ExecuteProcedure(UProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void ExecuteProcedureByIndex(int32 InProcedureIndex);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void ExecuteProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void CompleteProcedure(UProcedureBase* InProcedure, EProcedureExecuteResult InProcedureExecuteResult = EProcedureExecuteResult::Succeed);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void CompleteProcedureByIndex(int32 InProcedureIndex, EProcedureExecuteResult InProcedureExecuteResult = EProcedureExecuteResult::Succeed);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void CompleteProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass, EProcedureExecuteResult InProcedureExecuteResult = EProcedureExecuteResult::Succeed);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void LeaveProcedure(UProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void LeaveProcedureByIndex(int32 InProcedureIndex);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void LeaveProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass);
};
