// Fill out your copyright notice in the Description page of Project Settings.


#include "Video/VideoModuleNetworkComponent.h"

#include "Video/MediaPlayer/MediaPlayerBase.h"
#include "GameFramework/PlayerController.h"

namespace
{
	bool CanControlMedia(const UActorComponent* Component, const AMediaPlayerBase* Media)
	{
		const APlayerController* Controller = Component ? Cast<APlayerController>(Component->GetOwner()) : nullptr;
		return IsValid(Controller) && IsValid(Media) && Controller->GetWorld() == Media->GetWorld() && Media->IsOwnedBy(Controller);
	}
}

UVideoModuleNetworkComponent::UVideoModuleNetworkComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UVideoModuleNetworkComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	MaintainLocalWorldRoute();
}

bool UVideoModuleNetworkComponent::ServerPlayMovieMulticast_Validate(FGuid WorldRoute, AMediaPlayerBase* InMediaPlayer, const FName& InName) { return true; }
void UVideoModuleNetworkComponent::ServerPlayMovieMulticast_Implementation(FGuid WorldRoute, AMediaPlayerBase* InMediaPlayer, const FName& InName)
{
	if (!AcceptRemoteWorldRoute(WorldRoute) || !CanControlMedia(this, InMediaPlayer) || InName.IsNone()) return;
	InMediaPlayer->PlayMovie(InName, true);
}

bool UVideoModuleNetworkComponent::ServerStopMovieMulticast_Validate(FGuid WorldRoute, AMediaPlayerBase* InMediaPlayer, bool bSkip) { return true; }
void UVideoModuleNetworkComponent::ServerStopMovieMulticast_Implementation(FGuid WorldRoute, AMediaPlayerBase* InMediaPlayer, bool bSkip)
{
	if (!AcceptRemoteWorldRoute(WorldRoute) || !CanControlMedia(this, InMediaPlayer)) return;
	InMediaPlayer->StopMovie(bSkip, true);
}

bool UVideoModuleNetworkComponent::ServerPauseMovieMulticast_Validate(FGuid WorldRoute, AMediaPlayerBase* InMediaPlayer) { return true; }
void UVideoModuleNetworkComponent::ServerPauseMovieMulticast_Implementation(FGuid WorldRoute, AMediaPlayerBase* InMediaPlayer)
{
	if (!AcceptRemoteWorldRoute(WorldRoute) || !CanControlMedia(this, InMediaPlayer)) return;
	InMediaPlayer->PauseMovie(true);
}

bool UVideoModuleNetworkComponent::ServerSeekMovieMulticast_Validate(FGuid WorldRoute, AMediaPlayerBase* InMediaPlayer, const FTimespan& InTimespan) { return true; }
void UVideoModuleNetworkComponent::ServerSeekMovieMulticast_Implementation(FGuid WorldRoute, AMediaPlayerBase* InMediaPlayer, const FTimespan& InTimespan)
{
	if (!AcceptRemoteWorldRoute(WorldRoute) || !CanControlMedia(this, InMediaPlayer) || InTimespan < FTimespan::Zero()) return;
	InMediaPlayer->SeekMovie(InTimespan, true);
}
