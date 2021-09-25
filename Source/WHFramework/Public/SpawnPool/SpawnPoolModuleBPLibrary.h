// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "SpawnPoolModule.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SpawnPoolModuleBPLibrary.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API USpawnPoolModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static ASpawnPoolModule* SpawnPoolModuleInst;

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "SpawnPoolModuleBPLibrary")
	static ASpawnPoolModule* GetSpawnPoolModule(UObject* InWorldContext);

	template<class T>
	static T* SpawnActor(UObject* InWorldContext, TSubclassOf<AActor> InType = T::StaticClass())
	{
		if(ASpawnPoolModule* SpawnPoolModule = GetSpawnPoolModule(InWorldContext))
		{
			return SpawnPoolModule->SpawnActor<T>(InType);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext", DeterminesOutputType = "InType", DisplayName = "Spawn Actor"), Category = "SpawnPoolModuleBPLibrary")
	static AActor* K2_SpawnActor(UObject* InWorldContext, TSubclassOf<AActor> InType);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "SpawnPoolModuleBPLibrary")
	static void DespawnActor(UObject* InWorldContext, AActor* InActor);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "SpawnPoolModuleBPLibrary")
	static void ClearAllActor(UObject* InWorldContext);
};
