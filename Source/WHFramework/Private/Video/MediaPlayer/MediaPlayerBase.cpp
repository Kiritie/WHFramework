// Fill out your copyright notice in the Description page of Project Settings.


#include "Video/MediaPlayer/MediaPlayerBase.h"

#include "MediaPlayer.h"
#include "Main/MainModule.h"
#include "Video/VideoModuleStatics.h"
#include "Video/VideoModuleNetworkComponent.h"

AMediaPlayerBase::AMediaPlayerBase()
{
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif

	bReplicates = true;

	Name = NAME_None;

	MediaPlayer = nullptr;
}

void AMediaPlayerBase::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	UVideoModuleStatics::AddMediaPlayerToList(this);
	
	if(MediaPlayer)
	{
		MediaPlayer->OnEndReached.AddDynamic(this, &AMediaPlayerBase::OnMovieEndReached);
	}
}

void AMediaPlayerBase::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);
}

void AMediaPlayerBase::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AMediaPlayerBase::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);
}

void AMediaPlayerBase::PlayMovie_Implementation(const FName InMovieName, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiPlayMovie(InMovieName);
		}
		else if(UVideoModuleNetworkComponent* VideoModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UVideoModuleNetworkComponent>())
		{
			VideoModuleNetworkComponent->ServerPlayMovieMulticast(this, InMovieName);
		}
		return;
	}
	PlayMovieImpl(InMovieName);
}

void AMediaPlayerBase::PlayMovieWithDelegate_Implementation(const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnPlayFinished, bool bMulticast)
{
	OnMoviePlayFinishedSingleDelegate = InOnPlayFinished;

	PlayMovie(InMovieName, bMulticast);
}

void AMediaPlayerBase::PlayMovieImpl_Implementation(const FName InMovieName)
{
	if (MediaPlayer && MovieList.Contains(InMovieName))
	{
		MovieName = InMovieName;
		MediaPlayer->OpenSource(MovieList[InMovieName]);
		OnMoviePlayStartingDelegate.Broadcast(InMovieName);
	}
}

void AMediaPlayerBase::MultiPlayMovie_Implementation(const FName InMovieName)
{
	PlayMovie(InMovieName, false);
}

void AMediaPlayerBase::PauseMovie_Implementation(bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiPauseMovie();
		}
		else if(UVideoModuleNetworkComponent* VideoModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UVideoModuleNetworkComponent>())
		{
			VideoModuleNetworkComponent->ServerPauseMovieMulticast(this);
		}
		return;
	}
	PauseMovieImpl();
}

void AMediaPlayerBase::MultiPauseMovie_Implementation()
{
	PauseMovie(false);
}

void AMediaPlayerBase::PauseMovieImpl_Implementation()
{
	if(MediaPlayer)
	{
		MediaPlayer->Pause();
	}
}

void AMediaPlayerBase::SeekMovie_Implementation(const FTimespan& InTimespan, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiSeekMovie(InTimespan);
		}
		else if(UVideoModuleNetworkComponent* VideoModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UVideoModuleNetworkComponent>())
		{
			VideoModuleNetworkComponent->ServerSeekMovieMulticast(this, InTimespan);
		}
		return;
	}
	SeekMovieImpl(InTimespan);
}

void AMediaPlayerBase::MultiSeekMovie_Implementation(const FTimespan& InTimespan)
{
	SeekMovie(InTimespan, false);
}

void AMediaPlayerBase::SeekMovieImpl_Implementation(const FTimespan& InTimespan)
{
	if(MediaPlayer)
	{
		MediaPlayer->Seek(InTimespan);
	}
}

void AMediaPlayerBase::StopMovie_Implementation(bool bSkip, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiStopMovie(bSkip);
		}
		else if(UVideoModuleNetworkComponent* VideoModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UVideoModuleNetworkComponent>())
		{
			VideoModuleNetworkComponent->ServerStopMovieMulticast(this, bSkip);
		}
		return;
	}
	StopMovieImpl(bSkip);
}

void AMediaPlayerBase::StopMovieImpl_Implementation(bool bSkip)
{
	if(MediaPlayer)
	{
		MediaPlayer->Close();
	}
	if(bSkip && OnMoviePlayFinishedSingleDelegate.IsBound())
	{
		OnMoviePlayFinishedSingleDelegate.Clear();
	}
}

void AMediaPlayerBase::MultiStopMovie_Implementation(bool bSkip)
{
	StopMovie(bSkip, false);
}

void AMediaPlayerBase::OnMovieEndReached_Implementation()
{
	OnMoviePlayFinishedDelegate.Broadcast(MovieName);

	if (OnMoviePlayFinishedSingleDelegate.IsBound())
	{
		const auto& TempDelegate = OnMoviePlayFinishedSingleDelegate;
		OnMoviePlayFinishedSingleDelegate.Clear();
		TempDelegate.Execute(MovieName);
	}
}

void AMediaPlayerBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}
