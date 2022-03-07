// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

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
};
