// Fill out your copyright notice in the Description page of Project Settings.


#include "Media/MediaModuleBPLibrary.h"

#include "Media/MediaModule.h"

void UMediaModuleBPLibrary::AddMediaPlayerToList(AMediaPlayerBase* InMediaPlayer)
{
	if(AMediaModule* MediaModule = AMediaModule::Get())
	{
		MediaModule->AddMediaPlayerToList(InMediaPlayer);
	}
}

void UMediaModuleBPLibrary::RemoveMediaPlayerFromList(AMediaPlayerBase* InMediaPlayer)
{
	if(AMediaModule* MediaModule = AMediaModule::Get())
	{
		MediaModule->RemoveMediaPlayerFromList(InMediaPlayer);
	}
}

void UMediaModuleBPLibrary::RemoveMediaPlayerFromListByName(const FName InName)
{
	if(AMediaModule* MediaModule = AMediaModule::Get())
	{
		MediaModule->RemoveMediaPlayerFromListByName(InName);
	}
}

AMediaPlayerBase* UMediaModuleBPLibrary::GetMediaPlayerByName(const FName InName)
{
	if(AMediaModule* MediaModule = AMediaModule::Get())
	{
		return MediaModule->GetMediaPlayerByName(InName);
	}
	return nullptr;
}

void UMediaModuleBPLibrary::PlayMediaPlayerMovie(const FName InName, const FName InMovieName, bool bMulticast)
{
	if(AMediaModule* MediaModule = AMediaModule::Get())
	{
		MediaModule->PlayMediaPlayerMovie(InName, InMovieName, bMulticast);
	}
}

void UMediaModuleBPLibrary::PlayMediaPlayerMovieWithDelegate(const FName InName, const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnPlayFinished, bool bMulticast)
{
	if(AMediaModule* MediaModule = AMediaModule::Get())
	{
		MediaModule->PlayMovieWithDelegate(InName, InMovieName, InOnPlayFinished, bMulticast);
	}
}

void UMediaModuleBPLibrary::StopMediaPlayerMovie(const FName InName, bool bSkip, bool bMulticast)
{
	if(AMediaModule* MediaModule = AMediaModule::Get())
	{
		MediaModule->StopMediaPlayerMovie(InName, bSkip, bMulticast);
	}
}
