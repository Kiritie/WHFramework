// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "PawnModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PawnModuleStatics.generated.h"

class APawnBase;
class UPawnHandleBase;
class UPawnModule;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UPawnModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	template<class T>
	static T* GetCurrentPawn()
	{
		return Cast<T>(GetCurrentPawn());
	}

	static APawnBase* GetCurrentPawn();

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "PawnModuleStatics")
	static APawnBase* GetCurrentPawn(TSubclassOf<APawnBase> InClass);

	UFUNCTION(BlueprintCallable, Category = "PawnModuleStatics")
	static void SwitchPawn(APawnBase* InPawn, bool bResetCamera = true, bool bInstant = false);

	template<class T>
	static void SwitchPawnByClass(bool bResetCamera = true, bool bInstant = false, TSubclassOf<APawnBase> InClass = T::StaticClass())
	{
		SwitchPawnByClass(InClass, bResetCamera, bInstant);
	}

	UFUNCTION(BlueprintCallable, Category = "PawnModuleStatics")
	static void SwitchPawnByClass(TSubclassOf<APawnBase> InClass, bool bResetCamera = true, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "PawnModuleStatics")
	static void SwitchPawnByName(FName InName, bool bResetCamera = true, bool bInstant = false);

	template<class T>
	static bool HasPawnByClass(TSubclassOf<APawnBase> InClass = T::StaticClass())
	{
		return HasPawnByClass(InClass);
	}

	UFUNCTION(BlueprintPure, Category = "PawnModuleStatics")
	static bool HasPawnByClass(TSubclassOf<APawnBase> InClass);

	UFUNCTION(BlueprintPure, Category = "PawnModuleStatics")
	static bool HasPawnByName(FName InName);

	template<class T>
	static T* GetPawnByClass(TSubclassOf<APawnBase> InClass = T::StaticClass())
	{
		return GetPawnByClass(InClass);
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "PawnModuleStatics")
	static APawnBase* GetPawnByClass(TSubclassOf<APawnBase> InClass);

	UFUNCTION(BlueprintPure)
	static APawnBase* GetPawnByName(FName InName);

public:
	UFUNCTION(BlueprintCallable, Category = "PawnModuleStatics")
	static void AddPawnToList(APawnBase* InPawn);

	UFUNCTION(BlueprintCallable, Category = "PawnModuleStatics")
	static void RemovePawnFromList(APawnBase* InPawn);
};
