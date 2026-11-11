#include "Main/WorldNetworkRoute.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

AWorldNetworkRouteAuthority::AWorldNetworkRouteAuthority()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	bNetLoadOnClient = false;
	SetReplicateMovement(false);
	PrimaryActorTick.bCanEverTick = false;
}

void AWorldNetworkRouteAuthority::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		WorldEpoch = FGuid::NewGuid();
		ForceNetUpdate();
	}
}

void AWorldNetworkRouteAuthority::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWorldNetworkRouteAuthority, WorldEpoch);
}

FGuid WorldNetworkRoute::GetEpoch(UWorld* InWorld)
{
	if (!IsValid(InWorld) || InWorld->bIsTearingDown) return {};
	AWorldNetworkRouteAuthority* Authority = nullptr;
	for (TActorIterator<AWorldNetworkRouteAuthority> It(InWorld); It; ++It)
	{
		if (!IsValid(*It) || It->GetWorld() != InWorld) continue;
		if (Authority) return {};
		Authority = *It;
	}
	return Authority ? Authority->GetEpoch() : FGuid();
}
