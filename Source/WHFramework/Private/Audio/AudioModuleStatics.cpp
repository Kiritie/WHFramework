// Fill out your copyright notice in the Description page of Project Settings.


#include "Audio/AudioModuleStatics.h"

#include "Audio/AudioModule.h"

void UAudioModuleStatics::PlaySound2D(USoundBase* InSound, float InVolume, bool bMulticast)
{
	UAudioModule::Get().PlaySound2D(InSound, InVolume, bMulticast);
}

void UAudioModuleStatics::PlaySoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume, bool bMulticast)
{
	UAudioModule::Get().PlaySoundAtLocation(InSound, InLocation, InVolume, bMulticast);
}

FSingleSoundInfo UAudioModuleStatics::GetSingleSoundInfo(const FSingleSoundHandle& InHandle)
{
	return UAudioModule::Get().GetSingleSoundInfo(InHandle);
}

FSingleSoundHandle UAudioModuleStatics::PlaySingleSound2D(USoundBase* InSound, float InVolume, bool bMulticast)
{
	return UAudioModule::Get().PlaySingleSound2D(InSound, InVolume, bMulticast);
}

FSingleSoundHandle UAudioModuleStatics::PlaySingleSound2DWithDelegate(USoundBase* InSound, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, float InVolume, bool bMulticast)
{
	return UAudioModule::Get().PlaySingleSound2DWithDelegate(InSound, InOnSoundPlayFinish, InVolume, bMulticast);
}

FSingleSoundHandle UAudioModuleStatics::PlaySingleSoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume, bool bMulticast)
{
	return UAudioModule::Get().PlaySingleSoundAtLocation(InSound, InLocation, InVolume, bMulticast);
}

FSingleSoundHandle UAudioModuleStatics::PlaySingleSoundAtLocationWithDelegate(USoundBase* InSound, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, FVector InLocation, float InVolume, bool bMulticast)
{
	return UAudioModule::Get().PlaySingleSoundAtLocationWithDelegate(InSound, InOnSoundPlayFinish, InLocation, InVolume, bMulticast);
}

void UAudioModuleStatics::StopSingleSound(const FSingleSoundHandle& InHandle, bool bMulticast)
{
	UAudioModule::Get().StopSingleSound(InHandle, bMulticast);
}

void UAudioModuleStatics::SetSingleSoundPaused(const FSingleSoundHandle& InHandle, bool bPaused, bool bMulticast)
{
	UAudioModule::Get().SetSingleSoundPaused(InHandle, bPaused, bMulticast);
}

float UAudioModuleStatics::GetGlobalSoundVolume()
{
	return UAudioModule::Get().GetGlobalSoundVolume();
}

void UAudioModuleStatics::SetGlobalSoundVolume(float InVolume, float InFadeInTime)
{
	UAudioModule::Get().SetGlobalSoundVolume(InVolume, InFadeInTime);
}

float UAudioModuleStatics::GetGlobalSoundPitch()
{
	return UAudioModule::Get().GetGlobalSoundPitch();
}

void UAudioModuleStatics::SetGlobalSoundPitch(float InPitch, float InFadeInTime)
{
	UAudioModule::Get().SetGlobalSoundPitch(InPitch, InFadeInTime);
}

float UAudioModuleStatics::GetBackgroundSoundVolume()
{
	return UAudioModule::Get().GetBackgroundSoundVolume();
}

void UAudioModuleStatics::SetBackgroundSoundVolume(float InVolume, float InFadeInTime)
{
	UAudioModule::Get().SetBackgroundSoundVolume(InVolume, InFadeInTime);
}

float UAudioModuleStatics::GetBackgroundSoundPitch()
{
	return UAudioModule::Get().GetBackgroundSoundPitch();
}

void UAudioModuleStatics::SetBackgroundSoundPitch(float InPitch, float InFadeInTime)
{
	UAudioModule::Get().SetBackgroundSoundPitch(InPitch, InFadeInTime);
}

float UAudioModuleStatics::GetEnvironmentSoundVolume()
{
	return UAudioModule::Get().GetEnvironmentSoundVolume();
}

void UAudioModuleStatics::SetEnvironmentSoundVolume(float InVolume, float InFadeInTime)
{
	UAudioModule::Get().SetEnvironmentSoundVolume(InVolume, InFadeInTime);
}

float UAudioModuleStatics::GetEnvironmentSoundPitch()
{
	return UAudioModule::Get().GetEnvironmentSoundPitch();
}

void UAudioModuleStatics::SetEnvironmentSoundPitch(float InPitch, float InFadeInTime)
{
	UAudioModule::Get().SetEnvironmentSoundPitch(InPitch, InFadeInTime);
}

float UAudioModuleStatics::GetEffectSoundVolume()
{
	return UAudioModule::Get().GetEffectSoundVolume();
}

void UAudioModuleStatics::SetEffectSoundVolume(float InVolume, float InFadeInTime)
{
	UAudioModule::Get().SetEffectSoundVolume(InVolume, InFadeInTime);
}

float UAudioModuleStatics::GetEffectSoundPitch()
{
	return UAudioModule::Get().GetEffectSoundPitch();
}

void UAudioModuleStatics::SetEffectSoundPitch(float InPitch, float InFadeInTime)
{
	UAudioModule::Get().SetEffectSoundPitch(InPitch, InFadeInTime);
}
