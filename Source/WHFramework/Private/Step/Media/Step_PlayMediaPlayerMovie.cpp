// Fill out your copyright notice in the Description page of Project Settings.


#include "Step/Media/Step_PlayMediaPlayerMovie.h"

#include "Media/MediaModuleBPLibrary.h"
#include "Media/MediaPlayer/MediaPlayerBase.h"

UStep_PlayMediaPlayerMovie::UStep_PlayMediaPlayerMovie()
{
	StepName = FName("PlayMediaPlayerMovie");
	StepDisplayName = FText::FromString(TEXT("Media Movie"));

	StepType = EStepType::Standalone;

	PlayerName = NAME_None;
	MovieName = NAME_None;
}

void UStep_PlayMediaPlayerMovie::OnInitialize()
{
	Super::OnInitialize();
}

void UStep_PlayMediaPlayerMovie::OnEnter(UStepBase* InLastStep)
{
	Super::OnEnter(InLastStep);

	FOnMoviePlayFinishedSingleDelegate OnMoviePlayFinishedDelegate;
	OnMoviePlayFinishedDelegate.BindDynamic(this, &UStep_PlayMediaPlayerMovie::ServerOnMoviePlayFinished);
	UMediaModuleBPLibrary::PlayMediaPlayerMovieWithDelegate(PlayerName, MovieName, OnMoviePlayFinishedDelegate, true);

	UMediaModuleBPLibrary::PlayMediaPlayerMovie(PlayerName, MovieName, true);
}

void UStep_PlayMediaPlayerMovie::OnLeave()
{
	Super::OnLeave();

	UMediaModuleBPLibrary::StopMediaPlayerMovie(PlayerName, true, true);
}

void UStep_PlayMediaPlayerMovie::ServerOnMoviePlayFinished(const FName& InMovieName)
{
	Complete();
}
