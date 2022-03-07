// Fill out your copyright notice in the Description page of Project Settings.


#include "Audio/AudioModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Audio/AudioModule.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"

void UAudioModuleBPLibrary::PlaySound2D(USoundBase* InSound, float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		return AudioModule->PlaySound2D(InSound, InVolume, bMulticast);
	}
}

void UAudioModuleBPLibrary::PlaySoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		return AudioModule->PlaySoundAtLocation(InSound, InLocation, InVolume, bMulticast);
	}
}

void UAudioModuleBPLibrary::PlaySingleSound2D(USoundBase* InSound, float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		return AudioModule->PlaySingleSound2D(InSound, InVolume, bMulticast);
	}
}

void UAudioModuleBPLibrary::PlaySingleSoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		return AudioModule->PlaySingleSoundAtLocation(InSound, InLocation, InVolume, bMulticast);
	}
}

void UAudioModuleBPLibrary::StopSingleSound()
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		return AudioModule->StopSingleSound();
	}
}

void UAudioModuleBPLibrary::PlayGlobalBGSound()
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		return AudioModule->PlayGlobalBGSound();
	}
}

void UAudioModuleBPLibrary::PauseGlobalBGSound()
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		return AudioModule->PauseGlobalBGSound();
	}
}

void UAudioModuleBPLibrary::StopGlobalBGSound()
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		return AudioModule->StopGlobalBGSound();
	}
}

void UAudioModuleBPLibrary::InitSingleBGSound(USoundBase* InBGSound, float InBGVolume, bool bIsLoopSound, bool bIsUISound, bool bIsAutoPlay)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		return AudioModule->InitSingleBGSound(InBGSound, InBGVolume, bIsLoopSound, bIsUISound, bIsAutoPlay);
	}
}

void UAudioModuleBPLibrary::PlaySingleBGSound()
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		return AudioModule->PlaySingleBGSound();
	}
}

void UAudioModuleBPLibrary::PauseSingleBGSound()
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		return AudioModule->PauseSingleBGSound();
	}
}

void UAudioModuleBPLibrary::StopSingleBGSound()
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		return AudioModule->StopSingleBGSound();
	}
}
