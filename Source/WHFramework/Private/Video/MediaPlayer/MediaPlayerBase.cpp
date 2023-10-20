// Fill out your copyright notice in the Description page of Project Settings.


#include "Video/MediaPlayer/MediaPlayerBase.h"

#include "Main/MainModule.h"
#include "Video/VideoModuleBPLibrary.h"
#include "Video/VideoModuleNetworkComponent.h"

AMediaPlayerBase::AMediaPlayerBase()
{
	bReplicates = true;

	Name = NAME_None;

	ActorID = FGuid::NewGuid();
	Container = nullptr;
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
	
}

void AMediaPlayerBase::MultiPlayMovie_Implementation(const FName InMovieName)
{
	PlayMovie(InMovieName, false);
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
	if(bSkip && OnMoviePlayFinishedSingleDelegate.IsBound())
	{
		OnMoviePlayFinishedSingleDelegate.Clear();
	}
}

void AMediaPlayerBase::MultiStopMovie_Implementation(bool bSkip)
{
	StopMovie(bSkip, false);
}

void AMediaPlayerBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

}
