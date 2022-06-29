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

bool UAudioModuleNetworkComponent::ServerPlaySingleSound2DMulticast_Validate(USoundBase* InSound, FName InFlag, float InVolume) { return true; }
void UAudioModuleNetworkComponent::ServerPlaySingleSound2DMulticast_Implementation(USoundBase* InSound, FName InFlag, float InVolume)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->PlaySingleSound2D(InSound, InFlag, InVolume, true);
	}
}

bool UAudioModuleNetworkComponent::ServerPlaySingleSoundAtLocationMulticast_Validate(USoundBase* InSound, FName InFlag, FVector InLocation, float InVolume) { return true; }
void UAudioModuleNetworkComponent::ServerPlaySingleSoundAtLocationMulticast_Implementation(USoundBase* InSound, FName InFlag, FVector InLocation, float InVolume)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->PlaySingleSoundAtLocation(InSound, InFlag, InLocation, InVolume, true);
	}
}

bool UAudioModuleNetworkComponent::ServerStopSingleSoundMulticast_Validate(FName InFlag) { return true; }
void UAudioModuleNetworkComponent::ServerStopSingleSoundMulticast_Implementation(FName InFlag)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->StopSingleSound(InFlag, true);
	}
}

bool UAudioModuleNetworkComponent::ServerSetSingleSoundPausedMulticast_Validate(FName InFlag, bool bPaused) { return true; }
void UAudioModuleNetworkComponent::ServerSetSingleSoundPausedMulticast_Implementation(FName InFlag, bool bPaused)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->SetSingleSoundPaused(InFlag, bPaused, true);
	}
}

bool UAudioModuleNetworkComponent::ServerSetGlobalSoundVolumeMulticast_Validate(float InVolume) { return true; }
void UAudioModuleNetworkComponent::ServerSetGlobalSoundVolumeMulticast_Implementation(float InVolume)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->SetGlobalSoundVolume(InVolume, true);
	}
}

bool UAudioModuleNetworkComponent::ServerSetBGMSoundVolumeMulticast_Validate(float InVolume) { return true; }
void UAudioModuleNetworkComponent::ServerSetBGMSoundVolumeMulticast_Implementation(float InVolume)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->SetBGMSoundVolume(InVolume, true);
	}
}

bool UAudioModuleNetworkComponent::ServerSetEnvironmentSoundVolumeMulticast_Validate(float InVolume) { return true; }
void UAudioModuleNetworkComponent::ServerSetEnvironmentSoundVolumeMulticast_Implementation(float InVolume)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->SetEnvironmentSoundVolume(InVolume, true);
	}
}

bool UAudioModuleNetworkComponent::ServerSetEffectSoundVolumeMulticast_Validate(float InVolume) { return true; }
void UAudioModuleNetworkComponent::ServerSetEffectSoundVolumeMulticast_Implementation(float InVolume)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->SetEffectSoundVolume(InVolume, true);
	}
}
