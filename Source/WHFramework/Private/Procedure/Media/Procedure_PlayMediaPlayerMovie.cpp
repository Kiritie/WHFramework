// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Media/Procedure_PlayMediaPlayerMovie.h"

#include "Media/MediaModuleBPLibrary.h"
#include "Media/MediaPlayer/MediaPlayerBase.h"

UProcedure_PlayMediaPlayerMovie::UProcedure_PlayMediaPlayerMovie()
{
	ProcedureName = FName("PlayMediaPlayerMovie");
	ProcedureDisplayName = FText::FromString(TEXT("Media Movie"));

	ProcedureType = EProcedureType::Standalone;

	PlayerName = NAME_None;
	MovieName = NAME_None;
}

void UProcedure_PlayMediaPlayerMovie::OnInitialize()
{
	Super::OnInitialize();
}

void UProcedure_PlayMediaPlayerMovie::OnEnter(UProcedureBase* InLastProcedure)
{
	Super::OnEnter(InLastProcedure);

	FOnMoviePlayFinishedSingleDelegate OnMoviePlayFinishedDelegate;
	OnMoviePlayFinishedDelegate.BindDynamic(this, &UProcedure_PlayMediaPlayerMovie::ServerOnMoviePlayFinished);
	UMediaModuleBPLibrary::PlayMediaPlayerMovieWithDelegate(PlayerName, MovieName, OnMoviePlayFinishedDelegate, true);

	UMediaModuleBPLibrary::PlayMediaPlayerMovie(PlayerName, MovieName, true);
}

void UProcedure_PlayMediaPlayerMovie::OnLeave()
{
	Super::OnLeave();

	UMediaModuleBPLibrary::StopMediaPlayerMovie(PlayerName, true, true);
}

void UProcedure_PlayMediaPlayerMovie::ServerOnMoviePlayFinished(const FName& InMovieName)
{
	Complete();
}
