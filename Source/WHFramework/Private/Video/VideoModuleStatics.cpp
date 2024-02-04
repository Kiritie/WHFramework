// Fill out your copyright notice in the Description page of Project Settings.


#include "Video/VideoModuleStatics.h"

#include "Video/VideoModule.h"

void UVideoModuleStatics::AddMediaPlayerToList(AMediaPlayerBase* InMediaPlayer)
{
	UVideoModule::Get().AddMediaPlayerToList(InMediaPlayer);
}

void UVideoModuleStatics::RemoveMediaPlayerFromList(AMediaPlayerBase* InMediaPlayer)
{
	UVideoModule::Get().RemoveMediaPlayerFromList(InMediaPlayer);
}

void UVideoModuleStatics::RemoveMediaPlayerFromListByName(const FName InName)
{
	UVideoModule::Get().RemoveMediaPlayerFromListByName(InName);
}

AMediaPlayerBase* UVideoModuleStatics::GetMediaPlayerByName(const FName InName)
{
	return UVideoModule::Get().GetMediaPlayerByName(InName);
}

void UVideoModuleStatics::PlayMovieForMediaPlayer(const FName InName, const FName InMovieName, bool bMulticast)
{
	UVideoModule::Get().PlayMovieForMediaPlayer(InName, InMovieName, bMulticast);
}

void UVideoModuleStatics::PlayMovieForMediaPlayerWithDelegate(const FName InName, const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnPlayFinished, bool bMulticast)
{
	UVideoModule::Get().PlayMovieForMediaPlayerWithDelegate(InName, InMovieName, InOnPlayFinished, bMulticast);
}

void UVideoModuleStatics::PauseMovieForMediaPlayer(const FName InName, bool bMulticast)
{
	UVideoModule::Get().PauseMovieForMediaPlayer(InName, bMulticast);
}

void UVideoModuleStatics::SeekMovieForMediaPlayer(const FName InName, const FTimespan& InTimespan, bool bMulticast)
{
	UVideoModule::Get().SeekMovieForMediaPlayer(InName, InTimespan, bMulticast);
}

void UVideoModuleStatics::StopMovieForMediaPlayer(const FName InName, bool bSkip, bool bMulticast)
{
	UVideoModule::Get().StopMovieForMediaPlayer(InName, bSkip, bMulticast);
}
