// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/AudioModuleNetworkComponent.h"

#include "Audio/AudioModule.h"

UAudioModuleNetworkComponent::UAudioModuleNetworkComponent()
{
	
}

bool UAudioModuleNetworkComponent::ServerPlaySound2DMulticast_Validate(USoundBase* InSound, float InVolume) { return true; }
void UAudioModuleNetworkComponent::ServerPlaySound2DMulticast_Implementation(USoundBase* InSound, float InVolume)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		AudioModule->MultiPlaySound2D(InSound, InVolume);
	}
}

bool UAudioModuleNetworkComponent::ServerPlaySoundAtLocationMulticast_Validate(USoundBase* InSound, FVector InLocation, float InVolume) { return true; }
void UAudioModuleNetworkComponent::ServerPlaySoundAtLocationMulticast_Implementation(USoundBase* InSound, FVector InLocation, float InVolume)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		AudioModule->MultiPlaySoundAtLocation(InSound, InLocation, InVolume);
	}
}

bool UAudioModuleNetworkComponent::ServerPlaySingleSound2DMulticast_Validate(const FSingleSoundHandle& InHandle, USoundBase* InSound, float InVolume) { return true; }
void UAudioModuleNetworkComponent::ServerPlaySingleSound2DMulticast_Implementation(const FSingleSoundHandle& InHandle, USoundBase* InSound, float InVolume)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		AudioModule->MultiPlaySingleSound2D(InHandle, InSound, InVolume);
	}
}

bool UAudioModuleNetworkComponent::ServerPlaySingleSoundAtLocationMulticast_Validate(const FSingleSoundHandle& InHandle, USoundBase* InSound, FVector InLocation, float InVolume) { return true; }
void UAudioModuleNetworkComponent::ServerPlaySingleSoundAtLocationMulticast_Implementation(const FSingleSoundHandle& InHandle, USoundBase* InSound, FVector InLocation, float InVolume)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		AudioModule->MultiPlaySingleSoundAtLocation(InHandle, InSound, InLocation, InVolume);
	}
}

bool UAudioModuleNetworkComponent::ServerStopSingleSoundMulticast_Validate(const FSingleSoundHandle& InHandle) { return true; }
void UAudioModuleNetworkComponent::ServerStopSingleSoundMulticast_Implementation(const FSingleSoundHandle& InHandle)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		AudioModule->MultiStopSingleSound(InHandle);
	}
}

bool UAudioModuleNetworkComponent::ServerSetSingleSoundPausedMulticast_Validate(const FSingleSoundHandle& InHandle, bool bPaused) { return true; }
void UAudioModuleNetworkComponent::ServerSetSingleSoundPausedMulticast_Implementation(const FSingleSoundHandle& InHandle, bool bPaused)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		AudioModule->MultiSetSingleSoundPaused(InHandle, bPaused);
	}
}
