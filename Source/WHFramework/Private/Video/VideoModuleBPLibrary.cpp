// Fill out your copyright notice in the Description page of Project Settings.


#include "Video/VideoModuleBPLibrary.h"

#include "Video/VideoModule.h"

void UVideoModuleBPLibrary::AddMediaPlayerToList(AMediaPlayerBase* InMediaPlayer)
{
	if(AVideoModule* VideoModule = AVideoModule::Get())
	{
		VideoModule->AddMediaPlayerToList(InMediaPlayer);
	}
}

void UVideoModuleBPLibrary::RemoveMediaPlayerFromList(AMediaPlayerBase* InMediaPlayer)
{
	if(AVideoModule* VideoModule = AVideoModule::Get())
	{
		VideoModule->RemoveMediaPlayerFromList(InMediaPlayer);
	}
}

void UVideoModuleBPLibrary::RemoveMediaPlayerFromListByName(const FName InName)
{
	if(AVideoModule* VideoModule = AVideoModule::Get())
	{
		VideoModule->RemoveMediaPlayerFromListByName(InName);
	}
}

AMediaPlayerBase* UVideoModuleBPLibrary::GetMediaPlayerByName(const FName InName)
{
	if(AVideoModule* VideoModule = AVideoModule::Get())
	{
		return VideoModule->GetMediaPlayerByName(InName);
	}
	return nullptr;
}

void UVideoModuleBPLibrary::PlayMediaPlayerMovie(const FName InName, const FName InMovieName, bool bMulticast)
{
	if(AVideoModule* VideoModule = AVideoModule::Get())
	{
		VideoModule->PlayMediaPlayerMovie(InName, InMovieName, bMulticast);
	}
}

void UVideoModuleBPLibrary::PlayMediaPlayerMovieWithDelegate(const FName InName, const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnPlayFinished, bool bMulticast)
{
	if(AVideoModule* VideoModule = AVideoModule::Get())
	{
		VideoModule->PlayMovieWithDelegate(InName, InMovieName, InOnPlayFinished, bMulticast);
	}
}

void UVideoModuleBPLibrary::StopMediaPlayerMovie(const FName InName, bool bSkip, bool bMulticast)
{
	if(AVideoModule* VideoModule = AVideoModule::Get())
	{
		VideoModule->StopMediaPlayerMovie(InName, bSkip, bMulticast);
	}
}
