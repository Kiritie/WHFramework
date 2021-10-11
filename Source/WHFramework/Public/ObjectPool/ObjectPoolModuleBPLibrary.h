// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "MainModule.h"
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
	static T* SpawnObject(TSubclassOf<UObject> InType = T::StaticClass())
	{
		if(AObjectPoolModule* ObjectPoolModule = AMainModule::GetModuleByClass<AObjectPoolModule>())
		{
			return ObjectPoolModule->SpawnObject<T>(InType);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext", DeterminesOutputType = "InType", DisplayName = "Spawn Object"), Category = "ObjectPoolModuleBPLibrary")
	static UObject* K2_SpawnObject(TSubclassOf<UObject> InType);

	UFUNCTION(BlueprintCallable, Category = "ObjectPoolModuleBPLibrary")
	static void DespawnObject(UObject* InObject);

	UFUNCTION(BlueprintCallable, Category = "ObjectPoolModuleBPLibrary")
	static void ClearAllObject();
};
