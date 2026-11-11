// Fill out your copyright notice in the Description page of Project Settings.


#include "Pawn/PawnModuleNetworkComponent.h"

#include "Pawn/Base/PawnInterface.h"
#include "Pawn/Base/PawnBase.h"
#include "Pawn/PawnModule.h"
#include "Camera/Manager/CameraManagerBase.h"
#include "Camera/Mode/FollowCameraMode.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

namespace
{
	bool IsOwnedPawn(const UActorComponent* Component, const AActor* Actor)
	{
		const APlayerController* Controller = Component ? Cast<APlayerController>(Component->GetOwner()) : nullptr;
		return IsValid(Controller) && IsValid(Actor) && Controller->GetWorld() == Actor->GetWorld() && Controller->GetPawn() == Actor;
	}
}

UPawnModuleNetworkComponent::UPawnModuleNetworkComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPawnModuleNetworkComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	MaintainLocalWorldRoute();
	if (bPendingServerView) FlushPawnView();
}

void UPawnModuleNetworkComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(PendingViewTimer);
	PendingViewPawn.Reset();
	PendingViewWorld.Reset();
	PendingServerWorld.Reset();
	PendingServerPawn.Reset();
	bPendingServerView = false;
	Super::EndPlay(EndPlayReason);
}

bool UPawnModuleNetworkComponent::CanRequestPawnSwitch(const APlayerController* InController, const APawnBase* InPawn) const
{
	return IsValid(InController) && InController->GetWorld() == GetWorld() &&
		(!InPawn || (IsValid(InPawn) && InPawn->GetWorld() == GetWorld() &&
		(InController->GetPawn() == InPawn || InPawn->IsOwnedBy(InController))));
}

bool UPawnModuleNetworkComponent::RequestPawnSwitch(APawnBase* InPawn, bool bResetCamera, bool bInstant)
{
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (!Controller || !Controller->IsLocalController() || !GetWorld() || GetWorld()->bIsTearingDown ||
		(InPawn && InPawn->GetWorld() != GetWorld())) return false;
	const FGuid Route = MaintainLocalWorldRoute();
	if (!Route.IsValid()) return false;
	ServerRequestPawnSwitch(Route, InPawn, bResetCamera, bInstant);
	return true;
}

void UPawnModuleNetworkComponent::ServerRequestPawnSwitch_Implementation(FGuid WorldRoute, APawnBase* InPawn, bool bResetCamera, bool bInstant)
{
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (!AcceptRemoteWorldRoute(WorldRoute) || !Controller || !Controller->HasAuthority() || !CanRequestPawnSwitch(Controller, InPawn)) return;
	if (UPawnModule* Pawns = UPawnModule::GetPtr(GetWorld())) Pawns->SwitchPawn(Controller, InPawn, bResetCamera, bInstant);
}

void UPawnModuleNetworkComponent::NotifyPawnSwitchCommitted(APawnBase* InPawn, bool bResetCamera, bool bInstant)
{
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (!Controller || !Controller->HasAuthority() || !GetWorld() || GetWorld()->bIsTearingDown ||
		(InPawn && InPawn->GetWorld() != GetWorld())) return;
	if (Controller->IsLocalController())
	{
		bPendingServerView = false;
		QueuePawnView(InPawn, bResetCamera, bInstant);
		return;
	}
	PendingServerWorld = GetWorld();
	PendingServerPawn = InPawn;
	bPendingServerClear = InPawn == nullptr;
	bPendingServerReset = bResetCamera;
	bPendingServerInstant = bInstant;
	bPendingServerView = true;
	FlushPawnView();
}

void UPawnModuleNetworkComponent::FlushPawnView()
{
	UWorld* World = PendingServerWorld.Get();
	APawnBase* Pawn = PendingServerPawn.Get();
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (!bPendingServerView || !World || World != GetWorld() || World->bIsTearingDown || (!bPendingServerClear && !Pawn))
	{
		bPendingServerView = false;
		return;
	}
	if (!Controller || Controller->GetPawn() != Pawn)
	{
		bPendingServerView = false;
		return;
	}
	const FGuid Route = GetRemoteWorldRoute();
	if (!Route.IsValid()) return;
	bPendingServerView = false;
	ClientApplyPawnView(Route, Pawn, bPendingServerReset, bPendingServerInstant);
}

void UPawnModuleNetworkComponent::ClientApplyPawnView_Implementation(FGuid WorldRoute, APawnBase* InPawn, bool bResetCamera, bool bInstant)
{
	if (!AcceptLocalWorldRoute(WorldRoute)) return;
	QueuePawnView(InPawn, bResetCamera, bInstant);
}

void UPawnModuleNetworkComponent::QueuePawnView(APawnBase* InPawn, bool bResetCamera, bool bInstant)
{
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	UWorld* World = GetWorld();
	if (!Controller || !Controller->IsLocalController() || !World || World->bIsTearingDown ||
		(InPawn && InPawn->GetWorld() != World)) return;
	World->GetTimerManager().ClearTimer(PendingViewTimer);
	PendingViewPawn.Reset();
	PendingViewWorld.Reset();
	if (!bResetCamera) return;
	PendingViewWorld = World;
	PendingViewPawn = InPawn;
	bPendingViewClear = InPawn == nullptr;
	bPendingViewInstant = bInstant;
	World->GetTimerManager().SetTimer(PendingViewTimer, this, &ThisClass::ApplyPendingPawnView, 0.05f, true);
	ApplyPendingPawnView();
}

void UPawnModuleNetworkComponent::ApplyPendingPawnView()
{
	UWorld* World = PendingViewWorld.Get();
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	APawnBase* Pawn = PendingViewPawn.Get();
	if (!World || World != GetWorld() || World->bIsTearingDown || !Controller || !Controller->IsLocalController() ||
		(!bPendingViewClear && !Pawn))
	{
		if (World) World->GetTimerManager().ClearTimer(PendingViewTimer);
		return;
	}
	if (Controller->GetPawn() != Pawn) return;
	ACameraManagerBase* Camera = Cast<ACameraManagerBase>(Controller->PlayerCameraManager);
	if (!Camera) return;
	if (Pawn)
	{
		FCameraModeContext Context;
		Context.Target = Pawn;
		Context.Transition = bPendingViewInstant ? FCameraTransitionParams::Instant() : FCameraTransitionParams::Smooth();
		Camera->SetMode(UFollowCameraMode::StaticClass(), Context);
	}
	else Camera->ClearModeTarget();
	World->GetTimerManager().ClearTimer(PendingViewTimer);
	PendingViewPawn.Reset();
	PendingViewWorld.Reset();
}

bool UPawnModuleNetworkComponent::ServerTransformTowardsToMulticast_Validate(FGuid WorldRoute, AActor* InPawn, FTransform InTransform) { return true; }
void UPawnModuleNetworkComponent::ServerTransformTowardsToMulticast_Implementation(FGuid WorldRoute, AActor* InPawn, FTransform InTransform)
{
	if (!AcceptRemoteWorldRoute(WorldRoute) || !IsOwnedPawn(this, InPawn) || InTransform.ContainsNaN()) return;
	if (!CanRequestPawnOperation(Cast<APlayerController>(GetOwner()), InPawn)) return;
	if(IPawnInterface* Pawn = Cast<IPawnInterface>(InPawn))
	{
		Pawn->TransformTowards(InTransform, true);
	}
}

bool UPawnModuleNetworkComponent::ServerRotationTowardsMulticast_Validate(FGuid WorldRoute, AActor* InPawn, FRotator InRotation, float InDuration) { return true; }
void UPawnModuleNetworkComponent::ServerRotationTowardsMulticast_Implementation(FGuid WorldRoute, AActor* InPawn, FRotator InRotation, float InDuration)
{
	if (!AcceptRemoteWorldRoute(WorldRoute) || !IsOwnedPawn(this, InPawn) || InRotation.ContainsNaN() || !FMath::IsFinite(InDuration) || InDuration < 0.f) return;
	if (!CanRequestPawnOperation(Cast<APlayerController>(GetOwner()), InPawn)) return;
	if(IPawnInterface* Pawn = Cast<IPawnInterface>(InPawn))
	{
		Pawn->RotationTowards(InRotation, InDuration, true);
	}
}

bool UPawnModuleNetworkComponent::ServerAIMoveToMulticast_Validate(FGuid WorldRoute, AActor* InPawn, FVector InLocation, float InStopDistance) { return true; }
void UPawnModuleNetworkComponent::ServerAIMoveToMulticast_Implementation(FGuid WorldRoute, AActor* InPawn, FVector InLocation, float InStopDistance)
{
	if (!AcceptRemoteWorldRoute(WorldRoute) || !IsOwnedPawn(this, InPawn) || InLocation.ContainsNaN() || !FMath::IsFinite(InStopDistance) || InStopDistance < 0.f) return;
	if (!CanRequestPawnOperation(Cast<APlayerController>(GetOwner()), InPawn)) return;
	if(IPawnInterface* Pawn = Cast<IPawnInterface>(InPawn))
	{
		Pawn->AIMoveTo(InLocation, InStopDistance, true);
	}
}

bool UPawnModuleNetworkComponent::ServerStopAIMoveMulticast_Validate(FGuid WorldRoute, AActor* InPawn) { return true; }
void UPawnModuleNetworkComponent::ServerStopAIMoveMulticast_Implementation(FGuid WorldRoute, AActor* InPawn)
{
	if (!AcceptRemoteWorldRoute(WorldRoute) || !IsOwnedPawn(this, InPawn)) return;
	if (!CanRequestPawnOperation(Cast<APlayerController>(GetOwner()), InPawn)) return;
	if(IPawnInterface* Pawn = Cast<IPawnInterface>(InPawn))
	{
		Pawn->StopAIMove(true);
	}
}
