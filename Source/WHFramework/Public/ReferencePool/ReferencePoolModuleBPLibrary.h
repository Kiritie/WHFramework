// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Main/MainModule.h"
#include "ReferencePoolModule.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ReferencePoolModuleBPLibrary.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UReferencePoolModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	template<class T>
	static T* SpawnReference(TSubclassOf<AActor> InType = T::StaticClass())
	{
		if(AReferencePoolModule* ReferencePoolModule = AMainModule::GetModuleByClass<AReferencePoolModule>())
		{
			return ReferencePoolModule->SpawnReference<T>(InType);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InType", DisplayName = "Spawn Actor"), Category = "ReferencePoolModuleBPLibrary")
	static AActor* K2_SpawnReference(TSubclassOf<AActor> InType);

	UFUNCTION(BlueprintCallable, Category = "ReferencePoolModuleBPLibrary")
	static void DespawnReference(AActor* InActor);

	UFUNCTION(BlueprintCallable, Category = "ReferencePoolModuleBPLibrary")
	static void ClearAllActor();
};
