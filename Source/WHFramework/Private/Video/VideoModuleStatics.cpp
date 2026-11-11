// Fill out your copyright notice in the Description page of Project Settings.


#include "Video/VideoModuleStatics.h"

#include "Video/VideoModule.h"

void UVideoModuleStatics::AddMediaPlayerToList(const UObject* WorldContextObject, AMediaPlayerBase* InMediaPlayer)
{
	UVideoModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).AddMediaPlayerToList(InMediaPlayer);
}

void UVideoModuleStatics::RemoveMediaPlayerFromList(const UObject* WorldContextObject, AMediaPlayerBase* InMediaPlayer)
{
	UVideoModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).RemoveMediaPlayerFromList(InMediaPlayer);
}

void UVideoModuleStatics::RemoveMediaPlayerFromListByName(const UObject* WorldContextObject, const FName InName)
{
	UVideoModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).RemoveMediaPlayerFromListByName(InName);
}

AMediaPlayerBase* UVideoModuleStatics::GetMediaPlayerByName(const UObject* WorldContextObject, const FName InName)
{
	return UVideoModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).GetMediaPlayerByName(InName);
}

void UVideoModuleStatics::PlayMovieForMediaPlayer(const UObject* WorldContextObject, const FName InName, const FName InMovieName, bool bMulticast, APlayerController* InController)
{
	UVideoModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).PlayMovieForMediaPlayer(InName, InMovieName, bMulticast, InController);
}

void UVideoModuleStatics::PlayMovieForMediaPlayerWithDelegate(const UObject* WorldContextObject, const FName InName, const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnPlayFinished, bool bMulticast, APlayerController* InController)
{
	UVideoModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).PlayMovieForMediaPlayerWithDelegate(InName, InMovieName, InOnPlayFinished, bMulticast, InController);
}

void UVideoModuleStatics::PauseMovieForMediaPlayer(const UObject* WorldContextObject, const FName InName, bool bMulticast, APlayerController* InController)
{
	UVideoModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).PauseMovieForMediaPlayer(InName, bMulticast, InController);
}

void UVideoModuleStatics::SeekMovieForMediaPlayer(const UObject* WorldContextObject, const FName InName, const FTimespan& InTimespan, bool bMulticast, APlayerController* InController)
{
	UVideoModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).SeekMovieForMediaPlayer(InName, InTimespan, bMulticast, InController);
}

void UVideoModuleStatics::StopMovieForMediaPlayer(const UObject* WorldContextObject, const FName InName, bool bSkip, bool bMulticast, APlayerController* InController)
{
	UVideoModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).StopMovieForMediaPlayer(InName, bSkip, bMulticast, InController);
}
