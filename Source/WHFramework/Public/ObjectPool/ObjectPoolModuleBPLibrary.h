// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

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
	static AObjectPoolModule* ObjectPoolModuleInst;
	
	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "ObjectPoolModuleBPLibrary")
	static AObjectPoolModule* GetObjectPoolModule(UObject* InWorldContext);

	template<class T>
	static T* SpawnObject(UObject* InWorldContext, TSubclassOf<UObject> InType = nullptr)
	{
		if(AObjectPoolModule* ObjectPoolModule = GetObjectPoolModule(InWorldContext))
		{
			return ObjectPoolModule->SpawnObject<T>(InType);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext", DeterminesOutputType = "InType", DisplayName = "Spawn Object"), Category = "ObjectPoolModuleBPLibrary")
	static UObject* K2_SpawnObject(UObject* InWorldContext, TSubclassOf<UObject> InType);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "ObjectPoolModuleBPLibrary")
	static void DespawnObject(UObject* InWorldContext, UObject* InObject);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "ObjectPoolModuleBPLibrary")
	static void ClearAllObject(UObject* InWorldContext);
};
