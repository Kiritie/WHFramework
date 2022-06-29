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
		AudioModule->PlaySound2D(InSound, InVolume, bMulticast);
	}
}

void UAudioModuleBPLibrary::PlaySoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->PlaySoundAtLocation(InSound, InLocation, InVolume, bMulticast);
	}
}

void UAudioModuleBPLibrary::PlaySingleSound2D(USoundBase* InSound, FName InFlag, float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->PlaySingleSound2D(InSound, InFlag, InVolume, bMulticast);
	}
}

void UAudioModuleBPLibrary::PlaySingleSoundAtLocation(USoundBase* InSound, FName InFlag, FVector InLocation, float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->PlaySingleSoundAtLocation(InSound, InFlag, InLocation, InVolume, bMulticast);
	}
}

void UAudioModuleBPLibrary::PlaySingleSound2DWithDelegate(USoundBase* InSound, FName InFlag, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, float InVolume)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->PlaySingleSound2DWithDelegate(InSound, InFlag, InOnSoundPlayFinish, InVolume);
	}
}

void UAudioModuleBPLibrary::PlaySingleSoundAtLocationWithDelegate(USoundBase* InSound, FName InFlag, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, FVector InLocation, float InVolume)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->PlaySingleSoundAtLocationWithDelegate(InSound, InFlag, InOnSoundPlayFinish, InLocation, InVolume);
	}
}

void UAudioModuleBPLibrary::StopSingleSound(FName InFlag, bool bMulticast)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->StopSingleSound(InFlag, bMulticast);
	}
}

void UAudioModuleBPLibrary::SetSingleSoundPaused(FName InFlag, bool bPaused, bool bMulticast)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->SetSingleSoundPaused(InFlag, bPaused, bMulticast);
	}
}

void UAudioModuleBPLibrary::SetGlobalSoundVolume(float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->SetGlobalSoundVolume(InVolume, bMulticast);
	}
}

void UAudioModuleBPLibrary::SetBGMSoundVolume(float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->SetBGMSoundVolume(InVolume, bMulticast);
	}
}

void UAudioModuleBPLibrary::SetEnvironmentSoundVolume(float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->SetEnvironmentSoundVolume(InVolume, bMulticast);
	}
}

void UAudioModuleBPLibrary::SetEffectSoundVolume(float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->SetEffectSoundVolume(InVolume, bMulticast);
	}
}
