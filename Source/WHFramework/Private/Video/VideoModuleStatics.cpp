// Fill out your copyright notice in the Description page of Project Settings.


#include "Video/VideoModuleStatics.h"

#include "Video/VideoModule.h"

void UVideoModuleStatics::AddMediaPlayerToList(AMediaPlayerBase* InMediaPlayer)
{
	if(UVideoModule* VideoModule = UVideoModule::Get())
	{
		VideoModule->AddMediaPlayerToList(InMediaPlayer);
	}
}

void UVideoModuleStatics::RemoveMediaPlayerFromList(AMediaPlayerBase* InMediaPlayer)
{
	if(UVideoModule* VideoModule = UVideoModule::Get())
	{
		VideoModule->RemoveMediaPlayerFromList(InMediaPlayer);
	}
}

void UVideoModuleStatics::RemoveMediaPlayerFromListByName(const FName InName)
{
	if(UVideoModule* VideoModule = UVideoModule::Get())
	{
		VideoModule->RemoveMediaPlayerFromListByName(InName);
	}
}

AMediaPlayerBase* UVideoModuleStatics::GetMediaPlayerByName(const FName InName)
{
	if(UVideoModule* VideoModule = UVideoModule::Get())
	{
		return VideoModule->GetMediaPlayerByName(InName);
	}
	return nullptr;
}

void UVideoModuleStatics::PlayMediaPlayerMovie(const FName InName, const FName InMovieName, bool bMulticast)
{
	if(UVideoModule* VideoModule = UVideoModule::Get())
	{
		VideoModule->PlayMediaPlayerMovie(InName, InMovieName, bMulticast);
	}
}

void UVideoModuleStatics::PlayMediaPlayerMovieWithDelegate(const FName InName, const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnPlayFinished, bool bMulticast)
{
	if(UVideoModule* VideoModule = UVideoModule::Get())
	{
		VideoModule->PlayMovieWithDelegate(InName, InMovieName, InOnPlayFinished, bMulticast);
	}
}

void UVideoModuleStatics::StopMediaPlayerMovie(const FName InName, bool bSkip, bool bMulticast)
{
	if(UVideoModule* VideoModule = UVideoModule::Get())
	{
		VideoModule->StopMediaPlayerMovie(InName, bSkip, bMulticast);
	}
}
