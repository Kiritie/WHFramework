// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "AudioModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AudioModuleBPLibrary.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAudioModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void PlaySingleSound2D(USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void PlaySingleSoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume = 1.0f, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void StopSingleSound();
};
