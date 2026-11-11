// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Common/LanguageTypes.h"
#include "Audio/AudioModuleTypes.h"
#include "Common/CommonTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AudioModuleStatics.generated.h"

class APlayerController;

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAudioModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	////////////////////////////////////////////////////
	// GlobalSettings
	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static TArray<FLanguageType> GetAudioLanguageTypes(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void SetAudioLanguageTypes(const UObject* WorldContextObject, const TArray<FLanguageType>& InLanguageTypes);

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static int32 GetAudioLanguageType(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void SetAudioLanguageType(const UObject* WorldContextObject, int32 InLanguageType);

	//////////////////////////////////////////////////////////////////////////
	/// Sound
	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static FNetworkDispatchResult PlaySound2D(const UObject* WorldContextObject, USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false, APlayerController* InController = nullptr);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static FNetworkDispatchResult PlaySoundAtLocation(const UObject* WorldContextObject, USoundBase* InSound, FVector InLocation, float InVolume = 1.0f, bool bMulticast = false, APlayerController* InController = nullptr);

	//////////////////////////////////////////////////////////////////////////
	/// SingleSound
	UFUNCTION(BlueprintPure, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static FSingleSoundInfo GetSingleSoundInfo(const UObject* WorldContextObject, const FSingleSoundHandle& InHandle, APlayerController* InController = nullptr);

	UFUNCTION(BlueprintPure, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static EAudioNetworkSoundState GetSingleSoundRequestState(const UObject* WorldContextObject, const FSingleSoundHandle& InHandle, APlayerController* InController);

	UFUNCTION(BlueprintPure, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static FNetworkDispatchResult GetSingleSoundDispatchResult(const UObject* WorldContextObject, const FSingleSoundHandle& InHandle, APlayerController* InController);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static FSoundPlaybackResult PlaySingleSound2D(const UObject* WorldContextObject, USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false, APlayerController* InController = nullptr);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static FSoundPlaybackResult PlaySingleSound2DWithDelegate(const UObject* WorldContextObject, USoundBase* InSound, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, float InVolume = 1.0f, bool bMulticast = false, APlayerController* InController = nullptr);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static FSoundPlaybackResult PlaySingleSoundAtLocation(const UObject* WorldContextObject, USoundBase* InSound, FVector InLocation, float InVolume = 1.0f, bool bMulticast = false, APlayerController* InController = nullptr);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static FSoundPlaybackResult PlaySingleSoundAtLocationWithDelegate(const UObject* WorldContextObject, USoundBase* InSound, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, FVector InLocation, float InVolume = 1.0f, bool bMulticast = false, APlayerController* InController = nullptr);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static FNetworkDispatchResult StopSingleSound(const UObject* WorldContextObject, const FSingleSoundHandle& InHandle, bool bMulticast = false, APlayerController* InController = nullptr);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static FNetworkDispatchResult SetSingleSoundPaused(const UObject* WorldContextObject, const FSingleSoundHandle& InHandle, bool bPaused, bool bMulticast = false, APlayerController* InController = nullptr);

	//////////////////////////////////////////////////////////////////////////
	/// GlobalSound
	UFUNCTION(BlueprintPure, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static float GetGlobalSoundVolume(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void SetGlobalSoundVolume(const UObject* WorldContextObject, float InVolume = 1.f, float InFadeInTime = 1.0f);

	UFUNCTION(BlueprintPure, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static float GetGlobalSoundPitch(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void SetGlobalSoundPitch(const UObject* WorldContextObject, float InPitch = 1.f, float InFadeInTime = 1.0f);

	//////////////////////////////////////////////////////////////////////////
	/// BackgroundSound
	UFUNCTION(BlueprintPure, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static float GetBackgroundSoundVolume(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void SetBackgroundSoundVolume(const UObject* WorldContextObject, float InVolume = 1.f, float InFadeInTime = 1.0f);

	UFUNCTION(BlueprintPure, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static float GetBackgroundSoundPitch(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void SetBackgroundSoundPitch(const UObject* WorldContextObject, float InPitch = 1.f, float InFadeInTime = 1.0f);

	//////////////////////////////////////////////////////////////////////////
	/// EnvironmentSound
	UFUNCTION(BlueprintPure, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static float GetEnvironmentSoundVolume(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void SetEnvironmentSoundVolume(const UObject* WorldContextObject, float InVolume = 1.f, float InFadeInTime = 1.0f);

	UFUNCTION(BlueprintPure, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static float GetEnvironmentSoundPitch(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void SetEnvironmentSoundPitch(const UObject* WorldContextObject, float InPitch = 1.f, float InFadeInTime = 1.0f);

	//////////////////////////////////////////////////////////////////////////
	/// EffectSound
	UFUNCTION(BlueprintPure, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static float GetEffectSoundVolume(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void SetEffectSoundVolume(const UObject* WorldContextObject, float InVolume = 1.f, float InFadeInTime = 1.0f);

	UFUNCTION(BlueprintPure, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static float GetEffectSoundPitch(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "AudioModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void SetEffectSoundPitch(const UObject* WorldContextObject, float InPitch = 1.f, float InFadeInTime = 1.0f);
};
