// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/AudioModuleNetworkComponent.h"

#include "Audio/AudioModule.h"
#include "Audio/AudioModuleBPLibrary.h"
#include "Main/MainModule.h"

UAudioModuleNetworkComponent::UAudioModuleNetworkComponent()
{
	
}

bool UAudioModuleNetworkComponent::ServerPlaySound2DMulticast_Validate(USoundBase* InSound, float InVolume) { return true; }
void UAudioModuleNetworkComponent::ServerPlaySound2DMulticast_Implementation(USoundBase* InSound, float InVolume)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->PlaySound2D(InSound, InVolume, true);
	}
}

bool UAudioModuleNetworkComponent::ServerPlaySoundAtLocationMulticast_Validate(USoundBase* InSound, FVector InLocation, float InVolume) { return true; }
void UAudioModuleNetworkComponent::ServerPlaySoundAtLocationMulticast_Implementation(USoundBase* InSound, FVector InLocation, float InVolume)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->PlaySoundAtLocation(InSound, InLocation, InVolume, true);
	}
}
