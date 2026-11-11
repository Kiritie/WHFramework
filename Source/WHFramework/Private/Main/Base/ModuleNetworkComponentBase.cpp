// Fill out your copyright notice in the Description page of Project Settings.

#include "Main/Base/ModuleNetworkComponentBase.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Main/WorldNetworkRoute.h"

UModuleNetworkComponentBase::UModuleNetworkComponentBase()
{
	SetIsReplicatedByDefault(true);
	
}

FGuid UModuleNetworkComponentBase::MaintainLocalWorldRoute()
{
	UWorld* World = GetWorld();
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (!World || World->bIsTearingDown || !Controller || !Controller->IsLocalController() || Controller->HasAuthority()) return {};
	const FGuid Epoch = WorldNetworkRoute::GetEpoch(World);
	if (LocalRouteWorld.Get() != World || LocalWorldEpoch != Epoch)
	{
		LocalRouteWorld = World;
		LocalWorldEpoch = Epoch;
		LocalWorldRoute = Epoch.IsValid() ? FGuid::NewGuid() : FGuid();
		LastWorldRouteBind = -100.0;
		OnWorldRouteChanged();
	}
	if (!Epoch.IsValid() || !LocalWorldRoute.IsValid()) return {};
	const double Now = FPlatformTime::Seconds();
	if (Now - LastWorldRouteBind >= 1.0)
	{
		LastWorldRouteBind = Now;
		ServerBindWorldRoute(Epoch, LocalWorldRoute);
	}
	return LocalWorldRoute;
}

void UModuleNetworkComponentBase::ServerBindWorldRoute_Implementation(FGuid InWorldEpoch, FGuid InRoute)
{
	UWorld* World = GetWorld();
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (!World || World->bIsTearingDown || !Controller || !Controller->HasAuthority() ||
		!InWorldEpoch.IsValid() || WorldNetworkRoute::GetEpoch(World) != InWorldEpoch || !InRoute.IsValid()) return;
	if (RemoteRouteWorld.Get() != World || RemoteWorldEpoch != InWorldEpoch || RemoteWorldRoute != InRoute)
	{
		RemoteRouteWorld = World;
		RemoteWorldEpoch = InWorldEpoch;
		RemoteWorldRoute = InRoute;
		OnWorldRouteChanged();
	}
}

FGuid UModuleNetworkComponentBase::GetRemoteWorldRoute() const
{
	UWorld* World = GetWorld();
	return RemoteRouteWorld.IsValid() && RemoteRouteWorld.Get() == World &&
		RemoteWorldEpoch.IsValid() && WorldNetworkRoute::GetEpoch(World) == RemoteWorldEpoch ? RemoteWorldRoute : FGuid();
}

bool UModuleNetworkComponentBase::AcceptRemoteWorldRoute(const FGuid& InRoute) const
{
	return InRoute.IsValid() && InRoute == GetRemoteWorldRoute();
}

bool UModuleNetworkComponentBase::AcceptLocalWorldRoute(const FGuid& InRoute)
{
	return InRoute.IsValid() && InRoute == MaintainLocalWorldRoute();
}
