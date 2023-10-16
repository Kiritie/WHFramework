// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AudioModuleTypes.h"

#include "Main/Base/ModuleNetworkComponentBase.h"

#include "AudioModuleNetworkComponent.generated.h"

/**
* 
*/
UCLASS()
class WHFRAMEWORK_API UAudioModuleNetworkComponent : public UModuleNetworkComponentBase
{
	GENERATED_BODY()

public:
	UAudioModuleNetworkComponent();

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerPlaySound2DMulticast(USoundBase* InSound, float InVolume);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerPlaySoundAtLocationMulticast(USoundBase* InSound, FVector InLocation, float InVolume);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerPlaySingleSound2DMulticast(const FSingleSoundHandle& InHandle, USoundBase* InSound, float InVolume);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerPlaySingleSoundAtLocationMulticast(const FSingleSoundHandle& InHandle, USoundBase* InSound, FVector InLocation, float InVolume);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerStopSingleSoundMulticast(const FSingleSoundHandle& InHandle);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerSetSingleSoundPausedMulticast(const FSingleSoundHandle& InHandle, bool bPaused);
};
