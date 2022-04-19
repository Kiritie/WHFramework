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

void UProcedure_PlayMediaPlayerMovie::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void UProcedure_PlayMediaPlayerMovie::OnEnter_Implementation(UProcedureBase* InLastProcedure)
{
	Super::OnEnter_Implementation(InLastProcedure);

	FOnMoviePlayFinishedSingleDelegate OnMoviePlayFinishedDelegate;
	OnMoviePlayFinishedDelegate.BindDynamic(this, &UProcedure_PlayMediaPlayerMovie::ServerOnMoviePlayFinished);
	UMediaModuleBPLibrary::PlayMediaPlayerMovieWithDelegate(PlayerName, MovieName, OnMoviePlayFinishedDelegate, true);

	UMediaModuleBPLibrary::PlayMediaPlayerMovie(PlayerName, MovieName, true);
}

void UProcedure_PlayMediaPlayerMovie::OnLeave_Implementation()
{
	Super::OnLeave_Implementation();

	UMediaModuleBPLibrary::StopMediaPlayerMovie(PlayerName, true, true);
}

void UProcedure_PlayMediaPlayerMovie::ServerOnMoviePlayFinished(const FName& InMovieName)
{
	Complete();
}
