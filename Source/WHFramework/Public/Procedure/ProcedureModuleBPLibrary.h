// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

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
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InProcedureClass"), Category = "ProcedureModuleBPLibrary")
	UProcedureBase* GetCurrentProcedure(TSubclassOf<UProcedureBase> InProcedureClass = nullptr);

public:
	UFUNCTION(BlueprintPure, Category = "ProcedureModuleBPLibrary")
	static bool HasProcedureByIndex(int32 InProcedureIndex);

	template<class T>
	static T* GetProcedureByIndex(int32 InProcedureIndex)
	{
		if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
		{
			return ProcedureModule->GetProcedureByIndex<T>(InProcedureIndex);
		}
		return nullptr;
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetProcedureByIndex", DeterminesOutputType = "InProcedureIndex"))
	static UProcedureBase* GetProcedureByIndex(int32 InProcedureIndex, TSubclassOf<UProcedureBase> InProcedureClass);
	
	template<class T>
	static bool HasProcedureByClass()
	{
		if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
		{
			return ProcedureModule->HasProcedureByClass<T>(T::StaticClass());
		}
		return nullptr;
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "HasProcedureByClass"))
	static bool HasProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass);

	template<class T>
	static T* GetProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass = T::StaticClass())
	{
		if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
		{
			return ProcedureModule->GetProcedureByClass<T>(InProcedureClass);
		}
		return nullptr;
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetProcedureByClass", DeterminesOutputType = "InProcedureClass"))
	static UProcedureBase* GetProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass);
			
	UFUNCTION(BlueprintPure, Category = "ProcedureModuleBPLibrary")
	static bool IsCurrentProcedure(UProcedureBase* InProcedure);

	UFUNCTION(BlueprintPure, Category = "ProcedureModuleBPLibrary")
	static bool IsCurrentProcedureIndex(int32 InProcedureIndex);

	template<class T>
	static bool IsCurrentProcedureClass()
	{
		return IsCurrentProcedureClass(T::StaticClass());
	}

	UFUNCTION(BlueprintPure, Category = "ProcedureModuleBPLibrary")
	static bool IsCurrentProcedureClass(TSubclassOf<UProcedureBase> InProcedureClass)
	{
		if(AProcedureModule* ProcedureModule = AProcedureModule::Get())
		{
			return ProcedureModule->IsCurrentProcedureClass(InProcedureClass);
		}
		return false;
	}

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
