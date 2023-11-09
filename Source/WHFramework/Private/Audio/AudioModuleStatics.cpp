// Fill out your copyright notice in the Description page of Project Settings.


#include "Audio/AudioModuleStatics.h"

#include "Audio/AudioModule.h"

void UAudioModuleStatics::PlaySound2D(USoundBase* InSound, float InVolume, bool bMulticast)
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		AudioModule->PlaySound2D(InSound, InVolume, bMulticast);
	}
}

void UAudioModuleStatics::PlaySoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume, bool bMulticast)
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		AudioModule->PlaySoundAtLocation(InSound, InLocation, InVolume, bMulticast);
	}
}

FSingleSoundInfo UAudioModuleStatics::GetSingleSoundInfo(const FSingleSoundHandle& InHandle)
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		return AudioModule->GetSingleSoundInfo(InHandle);
	}
	return FSingleSoundInfo();
}

FSingleSoundHandle UAudioModuleStatics::PlaySingleSound2D(USoundBase* InSound, float InVolume, bool bMulticast)
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		return AudioModule->PlaySingleSound2D(InSound, InVolume, bMulticast);
	}
	return FSingleSoundHandle();
}

FSingleSoundHandle UAudioModuleStatics::PlaySingleSound2DWithDelegate(USoundBase* InSound, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, float InVolume, bool bMulticast)
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		return AudioModule->PlaySingleSound2DWithDelegate(InSound, InOnSoundPlayFinish, InVolume, bMulticast);
	}
	return FSingleSoundHandle();
}

FSingleSoundHandle UAudioModuleStatics::PlaySingleSoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume, bool bMulticast)
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		return AudioModule->PlaySingleSoundAtLocation(InSound, InLocation, InVolume, bMulticast);
	}
	return FSingleSoundHandle();
}

FSingleSoundHandle UAudioModuleStatics::PlaySingleSoundAtLocationWithDelegate(USoundBase* InSound, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, FVector InLocation, float InVolume, bool bMulticast)
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		return AudioModule->PlaySingleSoundAtLocationWithDelegate(InSound, InOnSoundPlayFinish, InLocation, InVolume, bMulticast);
	}
	return FSingleSoundHandle();
}

void UAudioModuleStatics::StopSingleSound(const FSingleSoundHandle& InHandle, bool bMulticast)
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		AudioModule->StopSingleSound(InHandle, bMulticast);
	}
}

void UAudioModuleStatics::SetSingleSoundPaused(const FSingleSoundHandle& InHandle, bool bPaused, bool bMulticast)
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		AudioModule->SetSingleSoundPaused(InHandle, bPaused, bMulticast);
	}
}

float UAudioModuleStatics::GetGlobalSoundVolume()
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		return AudioModule->GetGlobalSoundVolume();
	}
	return 0.f;
}

void UAudioModuleStatics::SetGlobalSoundVolume(float InVolume, float InFadeInTime)
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		AudioModule->SetGlobalSoundVolume(InVolume, InFadeInTime);
	}
}

float UAudioModuleStatics::GetGlobalSoundPitch()
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		return AudioModule->GetGlobalSoundPitch();
	}
	return 0.f;
}

void UAudioModuleStatics::SetGlobalSoundPitch(float InPitch, float InFadeInTime)
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		AudioModule->SetGlobalSoundPitch(InPitch, InFadeInTime);
	}
}

float UAudioModuleStatics::GetBackgroundSoundVolume()
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		return AudioModule->GetBackgroundSoundVolume();
	}
	return 0.f;
}

void UAudioModuleStatics::SetBackgroundSoundVolume(float InVolume, float InFadeInTime)
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		AudioModule->SetBackgroundSoundVolume(InVolume, InFadeInTime);
	}
}

float UAudioModuleStatics::GetBackgroundSoundPitch()
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		return AudioModule->GetBackgroundSoundPitch();
	}
	return 0.f;
}

void UAudioModuleStatics::SetBackgroundSoundPitch(float InPitch, float InFadeInTime)
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		AudioModule->SetBackgroundSoundPitch(InPitch, InFadeInTime);
	}
}

float UAudioModuleStatics::GetEnvironmentSoundVolume()
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		return AudioModule->GetEnvironmentSoundVolume();
	}
	return 0.f;
}

void UAudioModuleStatics::SetEnvironmentSoundVolume(float InVolume, float InFadeInTime)
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		AudioModule->SetEnvironmentSoundVolume(InVolume, InFadeInTime);
	}
}

float UAudioModuleStatics::GetEnvironmentSoundPitch()
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		return AudioModule->GetEnvironmentSoundPitch();
	}
	return 0.f;
}

void UAudioModuleStatics::SetEnvironmentSoundPitch(float InPitch, float InFadeInTime)
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		AudioModule->SetEnvironmentSoundPitch(InPitch, InFadeInTime);
	}
}

float UAudioModuleStatics::GetEffectSoundVolume()
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		return AudioModule->GetEffectSoundVolume();
	}
	return 0.f;
}

void UAudioModuleStatics::SetEffectSoundVolume(float InVolume, float InFadeInTime)
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		AudioModule->SetEffectSoundVolume(InVolume, InFadeInTime);
	}
}

float UAudioModuleStatics::GetEffectSoundPitch()
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		return AudioModule->GetEffectSoundPitch();
	}
	return 0.f;
}

void UAudioModuleStatics::SetEffectSoundPitch(float InPitch, float InFadeInTime)
{
	if(UAudioModule* AudioModule = UAudioModule::Get())
	{
		AudioModule->SetEffectSoundPitch(InPitch, InFadeInTime);
	}
}
