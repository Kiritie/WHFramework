// Fill out your copyright notice in the Description page of Project Settings.


#include "Video/VideoModuleNetworkComponent.h"

#include "Video/MediaPlayer/MediaPlayerBase.h"

UVideoModuleNetworkComponent::UVideoModuleNetworkComponent()
{
	
}

bool UVideoModuleNetworkComponent::ServerPlayMovieMulticast_Validate(AMediaPlayerBase* InMediaPlayer, const FName& InName) { return true; }
void UVideoModuleNetworkComponent::ServerPlayMovieMulticast_Implementation(AMediaPlayerBase* InMediaPlayer, const FName& InName)
{
	InMediaPlayer->PlayMovie(InName);
}

bool UVideoModuleNetworkComponent::ServerStopMovieMulticast_Validate(AMediaPlayerBase* InMediaPlayer, bool bSkip) { return true; }
void UVideoModuleNetworkComponent::ServerStopMovieMulticast_Implementation(AMediaPlayerBase* InMediaPlayer, bool bSkip)
{
	InMediaPlayer->StopMovie(bSkip);
}

bool UVideoModuleNetworkComponent::ServerPauseMovieMulticast_Validate(AMediaPlayerBase* InMediaPlayer) { return true; }
void UVideoModuleNetworkComponent::ServerPauseMovieMulticast_Implementation(AMediaPlayerBase* InMediaPlayer)
{
	InMediaPlayer->PauseMovie();
}

bool UVideoModuleNetworkComponent::ServerSeekMovieMulticast_Validate(AMediaPlayerBase* InMediaPlayer, const FTimespan& InTimespan) { return true; }
void UVideoModuleNetworkComponent::ServerSeekMovieMulticast_Implementation(AMediaPlayerBase* InMediaPlayer, const FTimespan& InTimespan)
{
	InMediaPlayer->SeekMovie(InTimespan);
}
