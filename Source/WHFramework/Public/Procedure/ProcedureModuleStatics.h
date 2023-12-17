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
	UFUNCTION(BlueprintPure, Category = "ProcedureModuleStatics")
	static UProcedureAsset* GetCurrentProcedureAsset();

	UFUNCTION(BlueprintCallable, Category = "ProcedureModuleStatics")
	static void SetCurrentProcedureAsset(UProcedureAsset* InProcedureAsset, bool bAutoSwitchFirst = false);

	template<class T>
	static T* GetCurrentProcedure()
	{
		return Cast<T>(GetCurrentProcedure());
	}
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "ProcedureModuleStatics")
	static UProcedureBase* GetCurrentProcedure(TSubclassOf<UProcedureBase> InClass = nullptr);

public:
	UFUNCTION(BlueprintPure, Category = "ProcedureModuleStatics")
	static bool HasProcedureByIndex(int32 InIndex);

	template<class T>
	static T* GetProcedureByIndex(int32 InIndex)
	{
		return UProcedureModule::Get().GetProcedureByIndex<T>(InIndex);
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetProcedureByIndex", DeterminesOutputType = "InIndex"))
	static UProcedureBase* GetProcedureByIndex(int32 InIndex, TSubclassOf<UProcedureBase> InClass);
	
	template<class T>
	static bool HasProcedureByClass()
	{
		return UProcedureModule::Get().HasProcedureByClass<T>(T::StaticClass());
	}
	UFUNCTION(BlueprintPure, meta = (DisplayName = "HasProcedureByClass"))
	static bool HasProcedureByClass(TSubclassOf<UProcedureBase> InClass);

	template<class T>
	static T* GetProcedureByClass(TSubclassOf<UProcedureBase> InClass = T::StaticClass())
	{
		return UProcedureModule::Get().GetProcedureByClass<T>(InClass);
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
		return UProcedureModule::Get().IsCurrentProcedureClass(InClass);
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
