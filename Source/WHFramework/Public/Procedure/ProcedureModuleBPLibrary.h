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
	template<class T>
	static T* GetCurrentProcedure()
	{
		return Cast<T>(GetCurrentProcedure());
	}

	UFUNCTION(BlueprintPure, Category = "ProcedureModuleBPLibrary")
	static UProcedureBase* GetCurrentProcedure();
	
public:
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
	static void SwitchProcedure(UProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void SwitchProcedureByIndex(int32 InProcedureIndex);

	template<class T>
	static void SwitchProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass = T::StaticClass())
	{
		SwitchProcedureByClass(InProcedureClass);
	}

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void SwitchProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void SwitchFirstProcedure();

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void SwitchLastProcedure();

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void SwitchNextProcedure();

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleBPLibrary")
	static void GuideCurrentProcedure();
};
