// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AudioModuleTypes.h"

#include "Main/Base/ModuleNetworkComponent.h"

#include "AudioModuleNetworkComponent.generated.h"

/**
* 
*/
UCLASS()
class WHFRAMEWORK_API UAudioModuleNetworkComponent : public UModuleNetworkComponent
{
	GENERATED_BODY()

public:
	UAudioModuleNetworkComponent();

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerPlaySound2DMulticast(USoundBase* InSound, float InVolume);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerPlaySoundAtLocationMulticast(USoundBase* InSound, FVector InLocation, float InVolume);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerPlaySingleSound2DMulticast(USoundBase* InSound, FName InFlag, float InVolume);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerPlaySingleSoundAtLocationMulticast(USoundBase* InSound, FName InFlag, FVector InLocation, float InVolume);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerStopSingleSoundMulticast(FName InFlag);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerSetSingleSoundPausedMulticast(FName InFlag, bool bPaused);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerSetGlobalSoundVolumeMulticast(float InVolume);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerSetBGMSoundVolumeMulticast(float InVolume);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerSetEnvironmentSoundVolumeMulticast(float InVolume);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerSetEffectSoundVolumeMulticast(float InVolume);
};
