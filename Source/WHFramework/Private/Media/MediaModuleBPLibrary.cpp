// Fill out your copyright notice in the Description page of Project Settings.


#include "Media/MediaModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "Media/MediaModule.h"
#include "Media/MediaModuleNetworkComponent.h"

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

void UMediaModuleBPLibrary::RemoveMediaPlayerFromListByName(const FName InMediaPlayerName)
{
	if(AMediaModule* MediaModule = AMediaModule::Get())
	{
		MediaModule->RemoveMediaPlayerFromListByName(InMediaPlayerName);
	}
}

AMediaPlayerBase* UMediaModuleBPLibrary::GetMediaPlayerByName(const FName InMediaPlayerName)
{
	if(AMediaModule* MediaModule = AMediaModule::Get())
	{
		return MediaModule->GetMediaPlayerByName(InMediaPlayerName);
	}
	return nullptr;
}

void UMediaModuleBPLibrary::PlayMediaPlayerMovie(const FName InMediaPlayerName, const FName InMovieName, bool bMulticast)
{
	if(AMediaModule* MediaModule = AMediaModule::Get())
	{
		MediaModule->PlayMediaPlayerMovie(InMediaPlayerName, InMovieName, bMulticast);
	}
}

void UMediaModuleBPLibrary::PlayMediaPlayerMovieWithDelegate(const FName InMediaPlayerName, const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnMoviePlayFinishedDelegate, bool bMulticast)
{
	if(AMediaModule* MediaModule = AMediaModule::Get())
	{
		MediaModule->PlayMovieWithDelegate(InMediaPlayerName, InMovieName, InOnMoviePlayFinishedDelegate, bMulticast);
	}
}

void UMediaModuleBPLibrary::StopMediaPlayerMovie(const FName InMediaPlayerName, bool bSkip, bool bMulticast)
{
	if(AMediaModule* MediaModule = AMediaModule::Get())
	{
		MediaModule->StopMediaPlayerMovie(InMediaPlayerName, bSkip, bMulticast);
	}
}
