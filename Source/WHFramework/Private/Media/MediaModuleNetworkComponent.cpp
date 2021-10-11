// Fill out your copyright notice in the Description page of Project Settings.


#include "Media/MediaModuleNetworkComponent.h"

#include "Media/MediaPlayer/MediaPlayerBase.h"

UMediaModuleNetworkComponent::UMediaModuleNetworkComponent()
{
	
}

bool UMediaModuleNetworkComponent::ServerPlayMovieMulticast_Validate(AMediaPlayerBase* InMediaPlayer, const FName& InName) { return true; }
void UMediaModuleNetworkComponent::ServerPlayMovieMulticast_Implementation(AMediaPlayerBase* InMediaPlayer, const FName& InName)
{
	InMediaPlayer->PlayMovie(InName);
}

bool UMediaModuleNetworkComponent::ServerStopMovieMulticast_Validate(AMediaPlayerBase* InMediaPlayer, bool bSkip) { return true; }
void UMediaModuleNetworkComponent::ServerStopMovieMulticast_Implementation(AMediaPlayerBase* InMediaPlayer, bool bSkip)
{
	InMediaPlayer->StopMovie(bSkip);
}
