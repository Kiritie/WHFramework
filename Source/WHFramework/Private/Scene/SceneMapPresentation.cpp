#include "Scene/SceneModule.h"
#include "ObjectPool/ObjectPoolModuleTypes.h"
#include "Widget/WorldWidgetTypes.h"
#include "Scene/SceneMapTypes.h"
#include "Scene/SceneAreaTypes.h"
#include "Scene/SceneMarkerTypes.h"

#include "Camera/CameraModuleStatics.h"
#include "Common/CommonModuleStatics.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Main/MainModule.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Scene/Capture/MiniMapCapture.h"
#include "Scene/ScenePlayerIdentityInterface.h"
#include "Scene/Widget/WidgetSceneWorldMarker.h"
#include "Widget/WidgetModule.h"

void USceneModule::RefreshMiniMapCapture()
{
	if (!IsBusinessAdmissionOpen()) return;
	if (bRefreshingMiniMapCaptures) return;
	TGuardValue<bool> RefreshGuard(bRefreshingMiniMapCaptures, true);
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (bSceneRuntimeClosing || !World || World->bIsTearingDown || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer ||
		MiniMapSource != EWorldMiniMapSource::SceneCapture || !::IsValid(MiniMapTexture) ||
		MiniMapTexture->SizeX <= 0 || MiniMapTexture->SizeY <= 0 || !GameInstance)
	{
		ClearMiniMapCaptures();
		return;
	}
	TSet<ULocalPlayer*> DesiredPlayers;
	const TArray<ULocalPlayer*> LocalPlayers = GameInstance->GetLocalPlayers();
	for (ULocalPlayer* LocalPlayer : LocalPlayers)
	{
		APlayerController* Player = LocalPlayer ? LocalPlayer->GetPlayerController(World) : nullptr;
		if (!Player || !Player->IsLocalController() || Player->GetWorld() != World) continue;
		DesiredPlayers.Add(LocalPlayer);
		if (::IsValid(MiniMapCaptures.FindRef(LocalPlayer)) && ::IsValid(MiniMapTargets.FindRef(LocalPlayer))) continue;
		AMiniMapCapture* Old = MiniMapCaptures.FindRef(LocalPlayer);
		MiniMapCaptures.Remove(LocalPlayer);
		MiniMapTargets.Remove(LocalPlayer);
		if (::IsValid(Old)) Old->Destroy();
		UTextureRenderTarget2D* Target = DuplicateObject<UTextureRenderTarget2D>(MiniMapTexture, this);
		if (!Target) continue;
		Target->SetFlags(RF_Transient);
		Target->UpdateResource();
		Target->UpdateResourceImmediate(true);
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Spawn.ObjectFlags |= RF_Transient;
		Spawn.Owner = GetModuleOwner();
		AMiniMapCapture* Capture = World->SpawnActor<AMiniMapCapture>(Spawn);
		if (!Capture) continue;
		Capture->AttachToActor(GetModuleOwner(), FAttachmentTransformRules::KeepWorldTransform);
		MiniMapTargets.Add(LocalPlayer, Target);
		MiniMapCaptures.Add(LocalPlayer, Capture);
	}
	TArray<AMiniMapCapture*> Removed;
	for (auto It = MiniMapCaptures.CreateIterator(); It; ++It)
	{
		if (DesiredPlayers.Contains(It.Key().Get())) continue;
		AMiniMapCapture* Capture = It.Value();
		MiniMapTargets.Remove(It.Key());
		It.RemoveCurrent();
		if (::IsValid(Capture)) Removed.Add(Capture);
	}
	for (AMiniMapCapture* Capture : Removed) Capture->Destroy();
}

void USceneModule::UpdateMiniMapCapture()
{
	if (!IsBusinessAdmissionOpen()) return;
	RefreshMiniMapCapture();
	TArray<TObjectPtr<ULocalPlayer>> Players;
	MiniMapCaptures.GetKeys(Players);
	for (ULocalPlayer* LocalPlayer : Players)
	{
		AMiniMapCapture* Actor = MiniMapCaptures.FindRef(LocalPlayer);
		APlayerController* Player = LocalPlayer ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
		UTextureRenderTarget2D* Target = MiniMapTargets.FindRef(LocalPlayer);
		USceneCaptureComponent2D* Capture = ::IsValid(Actor) ? Actor->GetCapture() : nullptr;
		if (!Capture) continue;
		FVector Location;
		const FSceneMapView View = ResolveMiniMapView(Player, &Location);
		const bool bEnabled = ::IsValid(Target) && Target->SizeX > 0 && Target->SizeY > 0 && View.Range > UE_SMALL_NUMBER;
		Capture->TextureTarget = bEnabled ? Target : nullptr;
		Capture->SetVisibility(bEnabled);
		if (Capture->IsActive() != bEnabled) Capture->SetActive(bEnabled);
		if (!bEnabled) continue;
		Actor->SetActorLocationAndRotation(Location, FRotator(0.f, View.Yaw - 90.f, 0.f));
		const float TextureAspect = static_cast<float>(Target->SizeX) / Target->SizeY;
		Capture->OrthoWidth = View.Range * FMath::Max(1.f, TextureAspect);
	}
}

void USceneModule::ClearMiniMapCaptures()
{
	TArray<TObjectPtr<AMiniMapCapture>> Captures;
	MiniMapCaptures.GenerateValueArray(Captures);
	MiniMapCaptures.Reset();
	MiniMapTargets.Reset();
	for (AMiniMapCapture* Capture : Captures) if (::IsValid(Capture)) Capture->Destroy();
}

AMiniMapCapture* USceneModule::GetMiniMapCapture(APlayerController* InPlayer) const
{
	return InPlayer && InPlayer->GetWorld() == GetWorld() && InPlayer->IsLocalController()
		? MiniMapCaptures.FindRef(InPlayer->GetLocalPlayer()) : nullptr;
}

void USceneModule::SetMiniMapSource(EWorldMiniMapSource InMiniMapSource)
{
	if (!IsBusinessAdmissionOpen()) return;
	const bool bChanged = MiniMapSource != InMiniMapSource;
	MiniMapSource = InMiniMapSource;
	RefreshMiniMapCapture();
	if (!IsBusinessAdmissionOpen()) return;
	if (bChanged) OnMapPresentationChanged.Broadcast();
}

void USceneModule::SetMapBackgroundWidgetClass(TSubclassOf<UWidgetSceneMapBase> InClass)
{
	if (!IsBusinessAdmissionOpen()) return;
	if (MapBackgroundWidgetClass == InClass) return;
	MapBackgroundWidgetClass = InClass;
	OnMapPresentationChanged.Broadcast();
}

void USceneModule::SetMiniMapMode(EWorldMiniMapMode InMiniMapMode)
{
	if (!IsBusinessAdmissionOpen()) return;
	const bool bChanged = MiniMapMode != InMiniMapMode;
	MiniMapMode = InMiniMapMode;
	RefreshMiniMapCapture();
	if (!IsBusinessAdmissionOpen()) return;
	if (bChanged) OnMapPresentationChanged.Broadcast();
}

void USceneModule::SetMiniMapTexture(UTextureRenderTarget2D* InMiniMapTexture)
{
	if (!IsBusinessAdmissionOpen()) return;
	const bool bChanged = MiniMapTexture != InMiniMapTexture;
	if (bChanged) ClearMiniMapCaptures();
	if (!IsBusinessAdmissionOpen()) return;
	MiniMapTexture = InMiniMapTexture;

	UpdateMiniMapCapture();
	if (!IsBusinessAdmissionOpen()) return;
	if (bChanged) OnMapPresentationChanged.Broadcast();
}

FSceneMapView USceneModule::GetMapView(ESceneMarkerChannel InChannel, APlayerController* InPlayer) const
{
	FSceneMapView View;
	if (InChannel == ESceneMarkerChannel::Compass)
	{
		if (!InPlayer || InPlayer->GetWorld() != GetWorld() || !InPlayer->IsLocalController()) return View;
		FVector Location;
		FRotator Rotation;
		InPlayer->GetPlayerViewPoint(Location, Rotation);
		View.Center = FVector2D(Location);
		View.Yaw = Rotation.Yaw;
		return View;
	}
	if(InChannel == ESceneMarkerChannel::Map)
	{
		if (InPlayer && InPlayer->GetWorld() == GetWorld() && InPlayer->IsLocalController())
		{
			if (const FSceneMapView* PlayerView = PlayerWorldMapViews.Find(InPlayer->GetLocalPlayer())) return *PlayerView;
		}
		View.Center = WorldMapCenter;
		View.Range = WorldMapRange;
		return View;
	}
	return InChannel == ESceneMarkerChannel::MiniMap ? ResolveMiniMapView(InPlayer) : View;
}

void USceneModule::SetPlayerWorldMapView(APlayerController* InPlayer, const FSceneMapView& InView)
{
	if (!IsBusinessAdmissionOpen()) return;
	ULocalPlayer* Player = InPlayer && InPlayer->GetWorld() == GetWorld() && InPlayer->IsLocalController()
		? InPlayer->GetLocalPlayer() : nullptr;
	if (!Player || InView.Center.ContainsNaN() || !FMath::IsFinite(InView.Range) || !FMath::IsFinite(InView.Yaw)) return;
	for (auto It = PlayerWorldMapViews.CreateIterator(); It; ++It) if (!It.Key().IsValid()) It.RemoveCurrent();
	FSceneMapView View = InView;
	View.Range = FMath::Clamp(View.Range, WorldMapMinRange, WorldMapMaxRange);
	PlayerWorldMapViews.Add(Player, View);
}

FSceneMapView USceneModule::ResolveMiniMapView(APlayerController* InPlayer, FVector* OutLocation) const
{
	FSceneMapView View;
	if(OutLocation) *OutLocation = FVector::ZeroVector;
	if(!InPlayer || InPlayer->GetWorld() != GetWorld() || !InPlayer->IsLocalController() || MiniMapSource == EWorldMiniMapSource::None || MiniMapMode == EWorldMiniMapMode::None ||
		!FMath::IsFinite(MiniMapRange) || MiniMapRange <= UE_SMALL_NUMBER) return View;
	if(MiniMapSource == EWorldMiniMapSource::SceneCapture &&
		(!::IsValid(MiniMapTexture) || MiniMapTexture->SizeX <= 0 || MiniMapTexture->SizeY <= 0 ||
		!FMath::IsFinite(MiniMapRange * FMath::Max(1.f, static_cast<float>(MiniMapTexture->SizeX) / MiniMapTexture->SizeY)))) return View;

	FVector Location = MiniMapPoint.GetLocation();
	float Yaw = MiniMapPoint.Rotator().Yaw;
	switch(MiniMapMode)
	{
		case EWorldMiniMapMode::ViewPoint:
		{
			if(const APlayerController* PlayerController = InPlayer)
			{
				if(const AActor* ViewTarget = PlayerController->GetViewTarget())
				{
					Location = ViewTarget->GetActorLocation();
					Yaw = ViewTarget->GetActorRotation().Yaw;
				}
			}
			break;
		}
		case EWorldMiniMapMode::CameraPoint:
		{
			FRotator Rotation;
			InPlayer->GetPlayerViewPoint(Location, Rotation);
			Yaw = Rotation.Yaw;
			break;
		}
		default: break;
	}
	if(Location.ContainsNaN() || !FMath::IsFinite(Yaw)) return View;
	if(OutLocation) *OutLocation = Location;
	View.Center = FVector2D(Location);
	View.Range = MiniMapRange;
	View.Yaw = bMiniMapRotatable ? Yaw : 0.f;
	return View;
}

FGuid USceneModule::GetPlayerMarkerOwnerId(APlayerController* InPlayer)
{
	APawn* Pawn = InPlayer ? InPlayer->GetPawn() : nullptr;
	return Pawn && Pawn->Implements<UScenePlayerIdentityInterface>()
		? IScenePlayerIdentityInterface::Execute_GetSceneMarkerPlayerId(Pawn) : FGuid();
}

FSceneMarkerView USceneModule::ResolveMarker(FGuid InPlayerID, const FSceneMarker& InMarker, const FVector& InViewLocation, float InViewYaw) const
{
	FSceneMarkerView View;
	View.Marker = InMarker;
	View.Location = InMarker.Location;
	if(InMarker.ActorID.IsValid())
	{
		if(AActor* const* Actor = SceneActorMap.Find(InMarker.ActorID); Actor != nullptr && ::IsValid(*Actor))
		{
			View.Location = (*Actor)->GetActorLocation();
			View.bTargetLoaded = true;
		}
	}
	else if(!InMarker.AreaName.IsNone())
	{
		const FSceneArea Area = GetSceneArea(InMarker.AreaName);
		if(!Area.AreaName.IsNone())
		{
			View.Location = Area.EntranceLocation.IsNearlyZero() ? FVector(Area.AreaCenter, InMarker.Location.Z) : Area.EntranceLocation;
			View.bTargetLoaded = true;
		}
	}
	View.Location += InMarker.Offset;
	const FVector2D Delta = FVector2D(View.Location - InViewLocation);
	View.Distance = Delta.Size();
	const float DirectionYaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
	View.Bearing = FMath::FindDeltaAngleDegrees(InViewYaw, DirectionYaw);
	View.bTracked = InPlayerID.IsValid() && GetTrackedMarker(InPlayerID) == InMarker.MarkerID;
	return View;
}

FSceneMarkerView USceneModule::GetMarkerView(FGuid InPlayerID, FGuid InMarkerID, FVector InViewLocation, float InViewYaw) const
{
	if(const FSceneMarker* Marker = Markers.Find(InMarkerID); Marker && (!Marker->OwnerPlayerId.IsValid() || Marker->OwnerPlayerId == InPlayerID))
		return ResolveMarker(InPlayerID, *Marker, InViewLocation, InViewYaw);
	return FSceneMarkerView();
}

bool USceneModule::IsMarkerVisibleToPlayer(FGuid InPlayerID, const FSceneMarker& InMarker, ESceneMarkerChannel InChannel) const
{
	if (InMarker.OwnerPlayerId.IsValid() && InMarker.OwnerPlayerId != InPlayerID) return false;
	const bool bTracked = InPlayerID.IsValid() && GetTrackedMarker(InPlayerID) == InMarker.MarkerID;
	if (InChannel != ESceneMarkerChannel::Map && InMarker.bNavigationOnlyWhenTracked && !bTracked) return false;
	return InMarker.HasChannel(InChannel) || (InChannel == ESceneMarkerChannel::Compass && bTracked && InMarker.HasChannel(ESceneMarkerChannel::Map));
}

TArray<FSceneMarkerView> USceneModule::GetMarkerViews(FGuid InPlayerID, ESceneMarkerChannel InChannel, FVector InViewLocation, float InViewYaw, bool bFilterByDistance) const
{
	TArray<FSceneMarkerView> Views;
	for(const auto& Iter : Markers)
	{
		if (!IsMarkerVisibleToPlayer(InPlayerID, Iter.Value, InChannel)) continue;
		FSceneMarkerView View = ResolveMarker(InPlayerID, Iter.Value, InViewLocation, InViewYaw);
		if(bFilterByDistance && (View.Distance < Iter.Value.MinDistance || (Iter.Value.MaxDistance > 0.f && View.Distance > Iter.Value.MaxDistance))) continue;
		Views.Add(MoveTemp(View));
	}
	Views.StableSort([](const FSceneMarkerView& A, const FSceneMarkerView& B)
	{
		return A.Marker.Priority == B.Marker.Priority ? A.Distance < B.Distance : A.Marker.Priority > B.Marker.Priority;
	});
	return Views;
}

void USceneModule::RefreshWorldMarkerWidgets()
{
	if (!IsBusinessAdmissionOpen()) return;
	if (bRefreshingWorldMarkerWidgets) return;
	TGuardValue<bool> RefreshGuard(bRefreshingWorldMarkerWidgets, true);
	UWorld* World = GetWorld();
	UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
	UWidgetModule* Widgets = World ? UWidgetModule::GetPtr(World) : nullptr;
	if (!Instance || !Widgets || !World->IsGameWorld()) return;
	const uint32 Generation = WorldMarkerWidgetsGeneration;
	const bool bDefinitionsDirty = bWorldMarkerWidgetsDirty;
	bWorldMarkerWidgetsDirty = false;
	const TArray<ULocalPlayer*> Players = Instance->GetLocalPlayers();
	TArray<UWidgetSceneWorldMarker*> Removed;
	for (auto PlayerIt = WorldMarkerWidgets.CreateIterator(); PlayerIt; ++PlayerIt)
	{
		ULocalPlayer* LocalPlayer = PlayerIt.Key();
		APlayerController* Controller = LocalPlayer ? LocalPlayer->GetPlayerController(World) : nullptr;
		const FGuid PlayerId = GetPlayerMarkerOwnerId(Controller);
		for (auto It = PlayerIt.Value().Widgets.CreateIterator(); It; ++It)
		{
			const FSceneMarker* Marker = Markers.Find(It.Key());
			UWidgetSceneWorldMarker* Widget = It.Value();
			if (!Players.Contains(LocalPlayer) || !Controller || !Marker || !IsMarkerVisibleToPlayer(PlayerId, *Marker, ESceneMarkerChannel::World) ||
				!::IsValid(Widget) || !Widget->GetWidgetTag().IsValid() || Widget->GetOwningPlayer() != Controller)
			{
				if (::IsValid(Widget) && Widget->GetWidgetTag().IsValid()) Removed.Add(Widget);
				It.RemoveCurrent();
			}
		}
		if (!Players.Contains(LocalPlayer) || !Controller) PlayerIt.RemoveCurrent();
	}
	for (UWidgetSceneWorldMarker* Widget : Removed)
	{
		Widgets->DestroyWorldWidget(Widget, EObjectDespawnMode::Recovery);
		if (!IsBusinessAdmissionOpen() || Generation != WorldMarkerWidgetsGeneration) return;
	}
	TArray<FGuid> MarkerIds;
	Markers.GetKeys(MarkerIds);
	for (ULocalPlayer* LocalPlayer : Players)
	{
		APlayerController* Controller = LocalPlayer ? LocalPlayer->GetPlayerController(World) : nullptr;
		if (!Controller || !Controller->PlayerCameraManager || !Widgets->GetWorldWidgetContainer(Controller)) continue;
		const FVector ViewLocation = Controller->PlayerCameraManager->GetCameraLocation();
		const float ViewYaw = Controller->PlayerCameraManager->GetCameraRotation().Yaw;
		const FGuid PlayerId = GetPlayerMarkerOwnerId(Controller);
		for (const FGuid& MarkerId : MarkerIds)
		{
			const FSceneMarker* Found = Markers.Find(MarkerId);
			if (!Found || !IsMarkerVisibleToPlayer(PlayerId, *Found, ESceneMarkerChannel::World)) continue;
			const FSceneMarker Marker = *Found;
			UWidgetSceneWorldMarker* Widget = WorldMarkerWidgets.FindOrAdd(LocalPlayer).Widgets.FindRef(MarkerId);
			const bool bCreated = !::IsValid(Widget);
			if (bCreated)
			{
				FWidgetSpawnParameter Spawn(this);
				Spawn.OwningPlayer = Controller;
				Widget = Widgets->CreateWorldWidget<UWidgetSceneWorldMarker>(FWorldWidgetMapping(Marker.Location + Marker.Offset), Spawn);
				if (!IsBusinessAdmissionOpen() || Generation != WorldMarkerWidgetsGeneration)
				{
					if (::IsValid(Widget) && Widget->GetWidgetTag().IsValid()) Widgets->DestroyWorldWidget(Widget);
					return;
				}
				if (::IsValid(Widget) && Widget->GetWidgetTag().IsValid()) WorldMarkerWidgets.FindOrAdd(LocalPlayer).Widgets.Add(MarkerId, Widget);
			}
			if (::IsValid(Widget) && Widget->GetWidgetTag().IsValid())
			{
				const FSceneMarkerView View = ResolveMarker(PlayerId, Marker, ViewLocation, ViewYaw);
				if (bCreated || bDefinitionsDirty) Widget->SetMarkerView(View);
				else Widget->UpdateMarkerState(View);
			}
			if (!IsBusinessAdmissionOpen() || Generation != WorldMarkerWidgetsGeneration) return;
		}
	}
}

void USceneModule::NotifySceneMarkersChanged()
{
	if (!IsBusinessAdmissionOpen()) return;
	bWorldMarkerWidgetsDirty = true;
	OnSceneMarkersChanged.Broadcast();
}

void USceneModule::ClearWorldMarkerWidgets()
{
	++WorldMarkerWidgetsGeneration;
	TArray<UWidgetSceneWorldMarker*> Removed;
	for (const auto& Player : WorldMarkerWidgets)
		for (const auto& Entry : Player.Value.Widgets) if (::IsValid(Entry.Value)) Removed.Add(Entry.Value);
	WorldMarkerWidgets.Reset();
	if (UWidgetModule* Widgets = UWidgetModule::GetPtr(GetWorld()))
		for (UWidgetSceneWorldMarker* Widget : Removed) Widgets->DestroyWorldWidget(Widget, EObjectDespawnMode::Recovery);
}
