// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


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
	//////////////////////////////////////////////////////////////////////////
	/// Sound
	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void PlaySound2D(USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void PlaySoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume = 1.0f, bool bMulticast = false);

	//////////////////////////////////////////////////////////////////////////
	/// SingleSound
	UFUNCTION(BlueprintPure, Category = "AudioModuleBPLibrary")
	static FSingleSoundInfo GetSingleSoundInfo(const FSingleSoundHandle& InHandle);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static FSingleSoundHandle PlaySingleSound2D(USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static FSingleSoundHandle PlaySingleSound2DWithDelegate(USoundBase* InSound, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, float InVolume = 1.0f, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static FSingleSoundHandle PlaySingleSoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume = 1.0f, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static FSingleSoundHandle PlaySingleSoundAtLocationWithDelegate(USoundBase* InSound, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, FVector InLocation, float InVolume = 1.0f, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void StopSingleSound(const FSingleSoundHandle& InHandle, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void SetSingleSoundPaused(const FSingleSoundHandle& InHandle, bool bPaused, bool bMulticast = false);

	//////////////////////////////////////////////////////////////////////////
	/// GlobalSound
	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void SetGlobalSoundVolume(float InVolume = 1.f, bool bMulticast = false);

	//////////////////////////////////////////////////////////////////////////
	/// BackgroundSound
	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void SetBackgroundSoundVolume(float InVolume = 1.f, bool bMulticast = false);

	//////////////////////////////////////////////////////////////////////////
	/// EnvironmentSound
	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void SetEnvironmentSoundVolume(float InVolume = 1.f, bool bMulticast = false);

	//////////////////////////////////////////////////////////////////////////
	/// EffectSound
	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void SetEffectSoundVolume(float InVolume = 1.f, bool bMulticast = false);
};
