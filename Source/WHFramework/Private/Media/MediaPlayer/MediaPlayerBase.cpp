// Fill out your copyright notice in the Description page of Project Settings.


#include "Media/MediaPlayer/MediaPlayerBase.h"

#include "Main/MainModule.h"
#include "Media/MediaModuleBPLibrary.h"
#include "Media/MediaModuleNetworkComponent.h"
#include "Scene/SceneModuleBPLibrary.h"
#include "Net/UnrealNetwork.h"

AMediaPlayerBase::AMediaPlayerBase()
{
	bReplicates = true;

	PlayerName = NAME_None;

	RootComponent = CreateDefaultSubobject<USceneComponent>(FName("RootComponent"));

	ScenePoint = CreateDefaultSubobject<USceneComponent>(FName("ScenePoint"));
	ScenePoint->SetupAttachment(RootComponent);
}

void AMediaPlayerBase::BeginPlay()
{
	Super::BeginPlay();

	USceneModuleBPLibrary::SetSceneObject(PlayerName, this);
}

void AMediaPlayerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

USceneComponent* AMediaPlayerBase::GetScenePoint() const
{
	return ScenePoint;
}

void AMediaPlayerBase::PlayMovie_Implementation(const FName InMovieName, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiPlayMovie(InMovieName);
		}
		else if(UMediaModuleNetworkComponent* MediaModuleNetworkComponent = AMainModule::GetModuleByClass<UMediaModuleNetworkComponent>())
		{
			MediaModuleNetworkComponent->ServerPlayMovieMulticast(this, InMovieName);
		}
		return;
	}
	PlayMovieImpl(InMovieName);
}

void AMediaPlayerBase::PlayMovieWithDelegate_Implementation(const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnMoviePlayFinishedDelegate, bool bMulticast)
{
	OnMoviePlayFinishedSingleDelegate = InOnMoviePlayFinishedDelegate;

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
		else if(UMediaModuleNetworkComponent* MediaModuleNetworkComponent = AMainModule::GetModuleByClass<UMediaModuleNetworkComponent>())
		{
			MediaModuleNetworkComponent->ServerStopMovieMulticast(this, bSkip);
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
