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

void UAudioModuleBPLibrary::PlaySingleSound2D(USoundBase* InSound, float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->PlaySingleSound2D(InSound, InVolume, bMulticast);
	}
}

void UAudioModuleBPLibrary::PlaySingleSound2DWithDelegate(USoundBase* InSound, const FOnSoundPlayFinishedDelegate& InOnSoundPlayFinished, float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->PlaySingleSound2DWithDelegate(InSound, InOnSoundPlayFinished, InVolume, bMulticast);
	}
}

void UAudioModuleBPLibrary::PlaySingleSoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->PlaySingleSoundAtLocation(InSound, InLocation, InVolume, bMulticast);
	}
}

void UAudioModuleBPLibrary::PlaySingleSoundAtLocation2DWithDelegate(USoundBase* InSound, const FOnSoundPlayFinishedDelegate& InOnSoundPlayFinished, FVector InLocation, float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->PlaySingleSoundAtLocation2DWithDelegate(InSound, InOnSoundPlayFinished, InLocation, InVolume, bMulticast);
	}
}

void UAudioModuleBPLibrary::StopSingleSound()
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->StopSingleSound();
	}
}

float UAudioModuleBPLibrary::GetSingleSoundVolume()
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		return AudioModule->GetSingleSoundVolume();
	}
	return 0.f;
}

void UAudioModuleBPLibrary::SetSingleSoundVolume(float InVolume)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->SetSingleSoundVolume(InVolume);
	}
}

void UAudioModuleBPLibrary::PlayGlobalBGSound()
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->PlayGlobalBGSound();
	}
}

void UAudioModuleBPLibrary::PauseGlobalBGSound()
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->PauseGlobalBGSound();
	}
}

void UAudioModuleBPLibrary::StopGlobalBGSound()
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->StopGlobalBGSound();
	}
}

float UAudioModuleBPLibrary::GetGlobalBGSoundVolume()
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		return AudioModule->GetGlobalBGSoundVolume();
	}
	return 0.f;
}

void UAudioModuleBPLibrary::SetGlobalBGSoundVolume(float InVolume)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->SetGlobalBGSoundVolume(InVolume);
	}
}

void UAudioModuleBPLibrary::InitSingleBGSound(USoundBase* InBGSound, float InBGVolume, bool bIsLoopSound, bool bIsUISound, bool bIsAutoPlay)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->InitSingleBGSound(InBGSound, InBGVolume, bIsLoopSound, bIsUISound, bIsAutoPlay);
	}
}

void UAudioModuleBPLibrary::PlaySingleBGSound()
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->PlaySingleBGSound();
	}
}

void UAudioModuleBPLibrary::PauseSingleBGSound()
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->PauseSingleBGSound();
	}
}

void UAudioModuleBPLibrary::StopSingleBGSound()
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->StopSingleBGSound();
	}
}

float UAudioModuleBPLibrary::GetSingleBGSoundVolume()
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		return AudioModule->GetSingleBGSoundVolume();
	}
	return 0.f;
}

void UAudioModuleBPLibrary::SetSingleBGSoundVolume(float InVolume)
{
	if(AAudioModule* AudioModule = AMainModule::GetModuleByClass<AAudioModule>())
	{
		AudioModule->SetSingleBGSoundVolume(InVolume);
	}
}
