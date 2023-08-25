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

AMediaModule::~AMediaModule()
{
	TERMINATION_MODULE(AMediaModule)
}

#if WITH_EDITOR
void AMediaModule::OnGenerate()
{
	Super::OnGenerate();
}

void AMediaModule::OnDestroy()
{
	Super::OnDestroy();
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

void AMediaModule::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);
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

void AMediaModule::RemoveMediaPlayerFromListByName(const FName InName)
{
	RemoveMediaPlayerFromList(GetMediaPlayerByName(InName));
}

AMediaPlayerBase* AMediaModule::GetMediaPlayerByName(const FName InName) const
{
	for (auto Iter : MediaPlayers)
	{
		if(Iter->GetPlayerName() == InName)
		{
			return Iter;
		}
	}
	return nullptr;
}

void AMediaModule::PlayMediaPlayerMovie(const FName InName, const FName InMovieName, bool bMulticast)
{
	if(AMediaPlayerBase* MediaPlayer = GetMediaPlayerByName(InName))
	{
		MediaPlayer->PlayMovie(InMovieName, bMulticast);
	}
}

void AMediaModule::PlayMovieWithDelegate(const FName InName, const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnPlayFinished, bool bMulticast)
{
	if(AMediaPlayerBase* MediaPlayer = GetMediaPlayerByName(InName))
	{
		MediaPlayer->PlayMovieWithDelegate(InMovieName, InOnPlayFinished, bMulticast);
	}
}

void AMediaModule::StopMediaPlayerMovie(const FName InName, bool bSkip, bool bMulticast)
{
	if(AMediaPlayerBase* MediaPlayer = GetMediaPlayerByName(InName))
	{
		MediaPlayer->StopMovie(bSkip, bMulticast);
	}
}

void AMediaModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMediaModule, MediaPlayers);
}
