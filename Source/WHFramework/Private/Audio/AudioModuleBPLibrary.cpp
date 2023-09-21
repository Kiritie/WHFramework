// Fill out your copyright notice in the Description page of Project Settings.


#include "Audio/AudioModuleBPLibrary.h"

#include "Audio/AudioModule.h"

void UAudioModuleBPLibrary::PlaySound2D(USoundBase* InSound, float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		AudioModule->PlaySound2D(InSound, InVolume, bMulticast);
	}
}

void UAudioModuleBPLibrary::PlaySoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		AudioModule->PlaySoundAtLocation(InSound, InLocation, InVolume, bMulticast);
	}
}

FSingleSoundInfo UAudioModuleBPLibrary::GetSingleSoundInfo(const FSingleSoundHandle& InHandle)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		return AudioModule->GetSingleSoundInfo(InHandle);
	}
	return FSingleSoundInfo();
}

FSingleSoundHandle UAudioModuleBPLibrary::PlaySingleSound2D(USoundBase* InSound, float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		return AudioModule->PlaySingleSound2D(InSound, InVolume, bMulticast);
	}
	return FSingleSoundHandle();
}

FSingleSoundHandle UAudioModuleBPLibrary::PlaySingleSound2DWithDelegate(USoundBase* InSound, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		return AudioModule->PlaySingleSound2DWithDelegate(InSound, InOnSoundPlayFinish, InVolume, bMulticast);
	}
	return FSingleSoundHandle();
}

FSingleSoundHandle UAudioModuleBPLibrary::PlaySingleSoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		return AudioModule->PlaySingleSoundAtLocation(InSound, InLocation, InVolume, bMulticast);
	}
	return FSingleSoundHandle();
}

FSingleSoundHandle UAudioModuleBPLibrary::PlaySingleSoundAtLocationWithDelegate(USoundBase* InSound, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, FVector InLocation, float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		return AudioModule->PlaySingleSoundAtLocationWithDelegate(InSound, InOnSoundPlayFinish, InLocation, InVolume, bMulticast);
	}
	return FSingleSoundHandle();
}

void UAudioModuleBPLibrary::StopSingleSound(const FSingleSoundHandle& InHandle, bool bMulticast)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		AudioModule->StopSingleSound(InHandle, bMulticast);
	}
}

void UAudioModuleBPLibrary::SetSingleSoundPaused(const FSingleSoundHandle& InHandle, bool bPaused, bool bMulticast)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		AudioModule->SetSingleSoundPaused(InHandle, bPaused, bMulticast);
	}
}

void UAudioModuleBPLibrary::SetGlobalSoundVolume(float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		AudioModule->SetGlobalSoundVolume(InVolume, bMulticast);
	}
}

void UAudioModuleBPLibrary::SetBackgroundSoundVolume(float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		AudioModule->SetBackgroundSoundVolume(InVolume, bMulticast);
	}
}

void UAudioModuleBPLibrary::SetEnvironmentSoundVolume(float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		AudioModule->SetEnvironmentSoundVolume(InVolume, bMulticast);
	}
}

void UAudioModuleBPLibrary::SetEffectSoundVolume(float InVolume, bool bMulticast)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		AudioModule->SetEffectSoundVolume(InVolume, bMulticast);
	}
}
