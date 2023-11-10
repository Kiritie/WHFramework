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

void UVideoModuleStatics::PlayMediaPlayerMovie(const FName InName, const FName InMovieName, bool bMulticast)
{
	UVideoModule::Get().PlayMediaPlayerMovie(InName, InMovieName, bMulticast);
}

void UVideoModuleStatics::PlayMediaPlayerMovieWithDelegate(const FName InName, const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnPlayFinished, bool bMulticast)
{
	UVideoModule::Get().PlayMovieWithDelegate(InName, InMovieName, InOnPlayFinished, bMulticast);
}

void UVideoModuleStatics::StopMediaPlayerMovie(const FName InName, bool bSkip, bool bMulticast)
{
	UVideoModule::Get().StopMediaPlayerMovie(InName, bSkip, bMulticast);
}
