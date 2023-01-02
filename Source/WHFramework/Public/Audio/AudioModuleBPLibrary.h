// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"

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
	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void PlaySingleSound2D(USoundBase* InSound, FName InFlag, float InVolume = 1.0f, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void PlaySingleSoundAtLocation(USoundBase* InSound, FName InFlag, FVector InLocation, float InVolume = 1.0f, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void PlaySingleSound2DWithDelegate(USoundBase* InSound, FName InFlag, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, float InVolume = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void PlaySingleSoundAtLocationWithDelegate(USoundBase* InSound, FName InFlag, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, FVector InLocation, float InVolume = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void StopSingleSound(FName InFlag, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void SetSingleSoundPaused(FName InFlag, bool bPaused, bool bMulticast = false);

	//////////////////////////////////////////////////////////////////////////
	/// GlobalSound
	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void SetGlobalSoundVolume(float InVolume, bool bMulticast = false);

	//////////////////////////////////////////////////////////////////////////
	/// BGMSound
	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void SetBGMSoundVolume(float InVolume, bool bMulticast = false);

	//////////////////////////////////////////////////////////////////////////
	/// EnvironmentSound
	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void SetEnvironmentSoundVolume(float InVolume, bool bMulticast = false);

	//////////////////////////////////////////////////////////////////////////
	/// EffectSound
	UFUNCTION(BlueprintCallable, Category = "AudioModuleBPLibrary")
	static void SetEffectSoundVolume(float InVolume, bool bMulticast = false);
};
