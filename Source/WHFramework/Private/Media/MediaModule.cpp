// Fill out your copyright notice in the Description page of Project Settings.


#include "Media/MediaModule.h"

#include "Media/MediaPlayer/MediaPlayerBase.h"
#include "Net/UnrealNetwork.h"
		
IMPLEMENTATION_MODULE(AMediaModule)

// Sets default values
AMediaModule::AMediaModule()
{
	ModuleName = FName("MediaModule");

	MediaPlayers = TArray<AMediaPlayerBase*>();
}

#if WITH_EDITOR
void AMediaModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void AMediaModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void AMediaModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AMediaModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);
}

void AMediaModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AMediaModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AMediaModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void AMediaModule::AddMediaPlayerToList(AMediaPlayerBase* InMediaPlayer)
{
	if(!MediaPlayers.Contains(InMediaPlayer))
	{
		MediaPlayers.Add(InMediaPlayer);
	}
}

void AMediaModule::RemoveMediaPlayerFromList(AMediaPlayerBase* InMediaPlayer)
{
	if(MediaPlayers.Contains(InMediaPlayer))
	{
		MediaPlayers.Remove(InMediaPlayer);
	}
}

void AMediaModule::RemoveMediaPlayerFromListByName(const FName InMediaPlayerName)
{
	RemoveMediaPlayerFromList(GetMediaPlayerByName(InMediaPlayerName));
}

AMediaPlayerBase* AMediaModule::GetMediaPlayerByName(const FName InMediaPlayerName) const
{
	for (auto Iter : MediaPlayers)
	{
		if(Iter->GetPlayerName() == InMediaPlayerName)
		{
			return Iter;
		}
	}
	return nullptr;
}

void AMediaModule::PlayMediaPlayerMovie(const FName InMediaPlayerName, const FName InMovieName, bool bMulticast)
{
	if(AMediaPlayerBase* MediaPlayer = GetMediaPlayerByName(InMediaPlayerName))
	{
		MediaPlayer->PlayMovie(InMovieName, bMulticast);
	}
}

void AMediaModule::PlayMovieWithDelegate(const FName InMediaPlayerName, const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnMoviePlayFinishedDelegate, bool bMulticast)
{
	if(AMediaPlayerBase* MediaPlayer = GetMediaPlayerByName(InMediaPlayerName))
	{
		MediaPlayer->PlayMovieWithDelegate(InMovieName, InOnMoviePlayFinishedDelegate, bMulticast);
	}
}

void AMediaModule::StopMediaPlayerMovie(const FName InMediaPlayerName, bool bSkip, bool bMulticast)
{
	if(AMediaPlayerBase* MediaPlayer = GetMediaPlayerByName(InMediaPlayerName))
	{
		MediaPlayer->StopMovie(bSkip, bMulticast);
	}
}

void AMediaModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMediaModule, MediaPlayers);
}
