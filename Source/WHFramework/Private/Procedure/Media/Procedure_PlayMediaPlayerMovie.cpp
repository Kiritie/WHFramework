// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Media/Procedure_PlayMediaPlayerMovie.h"

#include "Media/MediaModuleBPLibrary.h"
#include "Media/MediaPlayer/MediaPlayerBase.h"

AProcedure_PlayMediaPlayerMovie::AProcedure_PlayMediaPlayerMovie()
{
	ProcedureName = FName("PlayMediaPlayerMovie");
	ProcedureDisplayName = FText::FromString(TEXT("Media Movie"));

	ProcedureType = EProcedureType::Standalone;

	PlayerName = NAME_None;
	MovieName = NAME_None;
}

void AProcedure_PlayMediaPlayerMovie::ServerOnInitialize_Implementation()
{
	Super::ServerOnInitialize_Implementation();
}

void AProcedure_PlayMediaPlayerMovie::ServerOnEnter_Implementation(AProcedureBase* InLastProcedure)
{
	Super::ServerOnEnter_Implementation(InLastProcedure);

	FOnMoviePlayFinishedSingleDelegate OnMoviePlayFinishedDelegate;
	OnMoviePlayFinishedDelegate.BindDynamic(this, &AProcedure_PlayMediaPlayerMovie::ServerOnMoviePlayFinished);
	UMediaModuleBPLibrary::PlayMediaPlayerMovieWithDelegate(PlayerName, MovieName, OnMoviePlayFinishedDelegate, true);

	UMediaModuleBPLibrary::PlayMediaPlayerMovie(PlayerName, MovieName, true);
}

void AProcedure_PlayMediaPlayerMovie::ServerOnLeave_Implementation(AProcedureBase* InNextProcedure)
{
	Super::ServerOnLeave_Implementation(InNextProcedure);

	UMediaModuleBPLibrary::StopMediaPlayerMovie(PlayerName, true, true);
}

void AProcedure_PlayMediaPlayerMovie::ServerOnMoviePlayFinished(const FName& InMovieName)
{
	ServerComplete();
}
