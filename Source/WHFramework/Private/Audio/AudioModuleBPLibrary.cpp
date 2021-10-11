// Fill out your copyright notice in the Description page of Project Settings.


#include "Audio/AudioModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Audio/AudioModule.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"

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
