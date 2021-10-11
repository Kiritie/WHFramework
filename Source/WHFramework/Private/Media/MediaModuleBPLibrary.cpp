// Fill out your copyright notice in the Description page of Project Settings.


#include "Media/MediaModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "Media/MediaModule.h"
#include "Media/MediaModuleNetworkComponent.h"

void UMediaModuleBPLibrary::AddMediaPlayerToList(AMediaPlayerBase* InMediaPlayer)
{
	if(AMediaModule* MediaModule = AMainModule::GetModuleByClass<AMediaModule>())
	{
		MediaModule->AddMediaPlayerToList(InMediaPlayer);
	}
}

void UMediaModuleBPLibrary::RemoveMediaPlayerFromList(AMediaPlayerBase* InMediaPlayer)
{
	if(AMediaModule* MediaModule = AMainModule::GetModuleByClass<AMediaModule>())
	{
		MediaModule->RemoveMediaPlayerFromList(InMediaPlayer);
	}
}

void UMediaModuleBPLibrary::RemoveMediaPlayerFromListByName(const FName InMediaPlayerName)
{
	if(AMediaModule* MediaModule = AMainModule::GetModuleByClass<AMediaModule>())
	{
		MediaModule->RemoveMediaPlayerFromListByName(InMediaPlayerName);
	}
}

AMediaPlayerBase* UMediaModuleBPLibrary::GetMediaPlayerByName(const FName InMediaPlayerName)
{
	if(AMediaModule* MediaModule = AMainModule::GetModuleByClass<AMediaModule>())
	{
		return MediaModule->GetMediaPlayerByName(InMediaPlayerName);
	}
	return nullptr;
}

void UMediaModuleBPLibrary::PlayMediaPlayerMovie(const FName InMediaPlayerName, const FName InMovieName, bool bMulticast)
{
	if(AMediaModule* MediaModule = AMainModule::GetModuleByClass<AMediaModule>())
	{
		MediaModule->PlayMediaPlayerMovie(InMediaPlayerName, InMovieName, bMulticast);
	}
}

void UMediaModuleBPLibrary::PlayMediaPlayerMovieWithDelegate(const FName InMediaPlayerName, const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnMoviePlayFinishedDelegate, bool bMulticast)
{
	if(AMediaModule* MediaModule = AMainModule::GetModuleByClass<AMediaModule>())
	{
		MediaModule->PlayMovieWithDelegate(InMediaPlayerName, InMovieName, InOnMoviePlayFinishedDelegate, bMulticast);
	}
}

void UMediaModuleBPLibrary::StopMediaPlayerMovie(const FName InMediaPlayerName, bool bSkip, bool bMulticast)
{
	if(AMediaModule* MediaModule = AMainModule::GetModuleByClass<AMediaModule>())
	{
		MediaModule->StopMediaPlayerMovie(InMediaPlayerName, bSkip, bMulticast);
	}
}
