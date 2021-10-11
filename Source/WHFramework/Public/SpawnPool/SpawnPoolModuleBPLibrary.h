// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "MainModule.h"
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
	template<class T>
	static T* SpawnActor(TSubclassOf<AActor> InType = T::StaticClass())
	{
		if(ASpawnPoolModule* SpawnPoolModule = AMainModule::GetModuleByClass<ASpawnPoolModule>())
		{
			return SpawnPoolModule->SpawnActor<T>(InType);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InType", DisplayName = "Spawn Actor"), Category = "SpawnPoolModuleBPLibrary")
	static AActor* K2_SpawnActor(TSubclassOf<AActor> InType);

	UFUNCTION(BlueprintCallable, Category = "SpawnPoolModuleBPLibrary")
	static void DespawnActor(AActor* InActor);

	UFUNCTION(BlueprintCallable, Category = "SpawnPoolModuleBPLibrary")
	static void ClearAllActor();
};
