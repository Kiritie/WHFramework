// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Audio/AudioModuleTypes.h"
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
	static void PlaySound2D(USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void PlaySoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume = 1.0f, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void PlaySingleSound2D(USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void PlaySingleSoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume = 1.0f, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void StopSingleSound();

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void PlayGlobalBGSound();

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void PauseGlobalBGSound();

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void StopGlobalBGSound();
	
	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void InitSingleBGSound(USoundBase* InBGSound, float InBGVolume = 1.0f, bool bIsLoopSound = true, bool bIsUISound = false, bool bIsAutoPlay = true);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void PlaySingleBGSound();

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void PauseSingleBGSound();

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void StopSingleBGSound();
};
