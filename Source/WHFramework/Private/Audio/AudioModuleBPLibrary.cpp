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

float UAudioModuleBPLibrary::GetGlobalSoundVolume()
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		return AudioModule->GetGlobalSoundVolume();
	}
	return 0.f;
}

void UAudioModuleBPLibrary::SetGlobalSoundVolume(float InVolume, float InFadeInTime)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		AudioModule->SetGlobalSoundVolume(InVolume, InFadeInTime);
	}
}

float UAudioModuleBPLibrary::GetGlobalSoundPitch()
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		return AudioModule->GetGlobalSoundPitch();
	}
	return 0.f;
}

void UAudioModuleBPLibrary::SetGlobalSoundPitch(float InPitch, float InFadeInTime)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		AudioModule->SetGlobalSoundPitch(InPitch, InFadeInTime);
	}
}

float UAudioModuleBPLibrary::GetBackgroundSoundVolume()
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		return AudioModule->GetBackgroundSoundVolume();
	}
	return 0.f;
}

void UAudioModuleBPLibrary::SetBackgroundSoundVolume(float InVolume, float InFadeInTime)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		AudioModule->SetBackgroundSoundVolume(InVolume, InFadeInTime);
	}
}

float UAudioModuleBPLibrary::GetBackgroundSoundPitch()
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		return AudioModule->GetBackgroundSoundPitch();
	}
	return 0.f;
}

void UAudioModuleBPLibrary::SetBackgroundSoundPitch(float InPitch, float InFadeInTime)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		AudioModule->SetBackgroundSoundPitch(InPitch, InFadeInTime);
	}
}

float UAudioModuleBPLibrary::GetEnvironmentSoundVolume()
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		return AudioModule->GetEnvironmentSoundVolume();
	}
	return 0.f;
}

void UAudioModuleBPLibrary::SetEnvironmentSoundVolume(float InVolume, float InFadeInTime)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		AudioModule->SetEnvironmentSoundVolume(InVolume, InFadeInTime);
	}
}

float UAudioModuleBPLibrary::GetEnvironmentSoundPitch()
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		return AudioModule->GetEnvironmentSoundPitch();
	}
	return 0.f;
}

void UAudioModuleBPLibrary::SetEnvironmentSoundPitch(float InPitch, float InFadeInTime)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		AudioModule->SetEnvironmentSoundPitch(InPitch, InFadeInTime);
	}
}

float UAudioModuleBPLibrary::GetEffectSoundVolume()
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		return AudioModule->GetEffectSoundVolume();
	}
	return 0.f;
}

void UAudioModuleBPLibrary::SetEffectSoundVolume(float InVolume, float InFadeInTime)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		AudioModule->SetEffectSoundVolume(InVolume, InFadeInTime);
	}
}

float UAudioModuleBPLibrary::GetEffectSoundPitch()
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		return AudioModule->GetEffectSoundPitch();
	}
	return 0.f;
}

void UAudioModuleBPLibrary::SetEffectSoundPitch(float InPitch, float InFadeInTime)
{
	if(AAudioModule* AudioModule = AAudioModule::Get())
	{
		AudioModule->SetEffectSoundPitch(InPitch, InFadeInTime);
	}
}
