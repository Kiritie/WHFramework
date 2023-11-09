// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ProcedureModule.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "ProcedureModuleStatics.generated.h"

class UProcedureBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UProcedureModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	template<class T>
	static T* GetCurrentProcedure()
	{
		return Cast<T>(GetCurrentProcedure());
	}
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "ProcedureModuleStatics")
	UProcedureBase* GetCurrentProcedure(TSubclassOf<UProcedureBase> InClass = nullptr);

public:
	UFUNCTION(BlueprintPure, Category = "ProcedureModuleStatics")
	static bool HasProcedureByIndex(int32 InIndex);

	template<class T>
	static T* GetProcedureByIndex(int32 InIndex)
	{
		if(UProcedureModule* ProcedureModule = UProcedureModule::Get())
		{
			return ProcedureModule->GetProcedureByIndex<T>(InIndex);
		}
		return nullptr;
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetProcedureByIndex", DeterminesOutputType = "InIndex"))
	static UProcedureBase* GetProcedureByIndex(int32 InIndex, TSubclassOf<UProcedureBase> InClass);
	
	template<class T>
	static bool HasProcedureByClass()
	{
		if(UProcedureModule* ProcedureModule = UProcedureModule::Get())
		{
			return ProcedureModule->HasProcedureByClass<T>(T::StaticClass());
		}
		return nullptr;
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "HasProcedureByClass"))
	static bool HasProcedureByClass(TSubclassOf<UProcedureBase> InClass);

	template<class T>
	static T* GetProcedureByClass(TSubclassOf<UProcedureBase> InClass = T::StaticClass())
	{
		if(UProcedureModule* ProcedureModule = UProcedureModule::Get())
		{
			return ProcedureModule->GetProcedureByClass<T>(InClass);
		}
		return nullptr;
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetProcedureByClass", DeterminesOutputType = "InClass"))
	static UProcedureBase* GetProcedureByClass(TSubclassOf<UProcedureBase> InClass);
			
	UFUNCTION(BlueprintPure, Category = "ProcedureModuleStatics")
	static bool IsCurrentProcedure(UProcedureBase* InProcedure);

	UFUNCTION(BlueprintPure, Category = "ProcedureModuleStatics")
	static bool IsCurrentProcedureIndex(int32 InIndex);

	template<class T>
	static bool IsCurrentProcedureClass()
	{
		return IsCurrentProcedureClass(T::StaticClass());
	}

	UFUNCTION(BlueprintPure, Category = "ProcedureModuleStatics")
	static bool IsCurrentProcedureClass(TSubclassOf<UProcedureBase> InClass)
	{
		if(UProcedureModule* ProcedureModule = UProcedureModule::Get())
		{
			return ProcedureModule->IsCurrentProcedureClass(InClass);
		}
		return false;
	}

public:
	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleStatics")
	static void SwitchProcedure(UProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleStatics")
	static void SwitchProcedureByIndex(int32 InIndex);

	template<class T>
	static void SwitchProcedureByClass(TSubclassOf<UProcedureBase> InClass = T::StaticClass())
	{
		SwitchProcedureByClass(InClass);
	}

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleStatics")
	static void SwitchProcedureByClass(TSubclassOf<UProcedureBase> InClass);

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleStatics")
	static void SwitchFirstProcedure();

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleStatics")
	static void SwitchLastProcedure();

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleStatics")
	static void SwitchNextProcedure();

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleStatics")
	static void GuideCurrentProcedure();
};
