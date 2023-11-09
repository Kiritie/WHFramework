// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Audio/AudioModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AudioModuleStatics.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAudioModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////
	/// Sound
	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics")
	static void PlaySound2D(USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics")
	static void PlaySoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume = 1.0f, bool bMulticast = false);

	//////////////////////////////////////////////////////////////////////////
	/// SingleSound
	UFUNCTION(BlueprintPure, Category = "AudioModuleStatics")
	static FSingleSoundInfo GetSingleSoundInfo(const FSingleSoundHandle& InHandle);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics")
	static FSingleSoundHandle PlaySingleSound2D(USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics")
	static FSingleSoundHandle PlaySingleSound2DWithDelegate(USoundBase* InSound, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, float InVolume = 1.0f, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics")
	static FSingleSoundHandle PlaySingleSoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume = 1.0f, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics")
	static FSingleSoundHandle PlaySingleSoundAtLocationWithDelegate(USoundBase* InSound, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, FVector InLocation, float InVolume = 1.0f, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics")
	static void StopSingleSound(const FSingleSoundHandle& InHandle, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics")
	static void SetSingleSoundPaused(const FSingleSoundHandle& InHandle, bool bPaused, bool bMulticast = false);

	//////////////////////////////////////////////////////////////////////////
	/// GlobalSound
public:
	UFUNCTION(BlueprintPure, Category = "AudioModuleStatics")
	static float GetGlobalSoundVolume();

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics")
	static void SetGlobalSoundVolume(float InVolume = 1.f, float InFadeInTime = 1.0f);

	UFUNCTION(BlueprintPure, Category = "AudioModuleStatics")
	static float GetGlobalSoundPitch();

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics")
	static void SetGlobalSoundPitch(float InPitch = 1.f, float InFadeInTime = 1.0f);

	//////////////////////////////////////////////////////////////////////////
	/// BackgroundSound
	UFUNCTION(BlueprintPure, Category = "AudioModuleStatics")
	static float GetBackgroundSoundVolume();

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics")
	static void SetBackgroundSoundVolume(float InVolume = 1.f, float InFadeInTime = 1.0f);

	UFUNCTION(BlueprintPure, Category = "AudioModuleStatics")
	static float GetBackgroundSoundPitch();

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics")
	static void SetBackgroundSoundPitch(float InPitch = 1.f, float InFadeInTime = 1.0f);

	//////////////////////////////////////////////////////////////////////////
	/// EnvironmentSound
	UFUNCTION(BlueprintPure, Category = "AudioModuleStatics")
	static float GetEnvironmentSoundVolume();

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics")
	static void SetEnvironmentSoundVolume(float InVolume = 1.f, float InFadeInTime = 1.0f);

	UFUNCTION(BlueprintPure, Category = "AudioModuleStatics")
	static float GetEnvironmentSoundPitch();

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics")
	static void SetEnvironmentSoundPitch(float InPitch = 1.f, float InFadeInTime = 1.0f);

	//////////////////////////////////////////////////////////////////////////
	/// EffectSound
	UFUNCTION(BlueprintPure, Category = "AudioModuleStatics")
	static float GetEffectSoundVolume();

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics")
	static void SetEffectSoundVolume(float InVolume = 1.f, float InFadeInTime = 1.0f);

	UFUNCTION(BlueprintPure, Category = "AudioModuleStatics")
	static float GetEffectSoundPitch();

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics")
	static void SetEffectSoundPitch(float InPitch = 1.f, float InFadeInTime = 1.0f);
};
