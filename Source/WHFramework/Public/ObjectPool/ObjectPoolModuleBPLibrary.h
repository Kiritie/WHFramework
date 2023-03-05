// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"

#include "Main/MainModule.h"
#include "ObjectPoolModule.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ObjectPoolModuleBPLibrary.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UObjectPoolModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	template<class T>
	static T* SpawnObject(const TArray<FParameter>* InParams = nullptr, TSubclassOf<UObject> InType = T::StaticClass())
	{
		if(AObjectPoolModule* ObjectPoolModule = AObjectPoolModule::Get())
		{
			return ObjectPoolModule->SpawnObject<T>(InParams, InType);
		}
		return nullptr;
	}

	template<class T>
	static T* SpawnObject(const TArray<FParameter>& InParams, TSubclassOf<UObject> InType = T::StaticClass())
	{
		if(AObjectPoolModule* ObjectPoolModule = AObjectPoolModule::Get())
		{
			return ObjectPoolModule->SpawnObject<T>(InParams, InType);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext", DeterminesOutputType = "InType", AutoCreateRefTerm = "InParams"), Category = "ObjectPoolModuleBPLibrary")
	static UObject* SpawnObject(TSubclassOf<UObject> InType, const TArray<FParameter>& InParams);

	UFUNCTION(BlueprintCallable, Category = "ObjectPoolModuleBPLibrary")
	static void DespawnObject(UObject* InObject, bool bRecovery = true);

	template<class T>
	static void ClearObject(TSubclassOf<UObject> InType = T::StaticClass())
	{
		if(AObjectPoolModule* ObjectPoolModule = AObjectPoolModule::Get())
		{
			ObjectPoolModule->ClearObject<T>(InType);
		}
	}

	UFUNCTION(BlueprintCallable, Category = "ObjectPoolModuleBPLibrary")
	static void ClearObject(TSubclassOf<UObject> InType);

	UFUNCTION(BlueprintCallable, Category = "ObjectPoolModuleBPLibrary")
	static void ClearAllObject();
};
