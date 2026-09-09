#include "Scene/Widget/WidgetSceneMarkerPanel.h"

#include "Camera/CameraModuleStatics.h"
#include "Common/CommonModuleStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Blueprint/WidgetTree.h"
#include "Scene/SceneModule.h"
#include "Scene/Capture/MiniMapCapture.h"
#include "Scene/SceneModuleStatics.h"
#include "Scene/Widget/WidgetSceneMapBase.h"
#include "Scene/Widget/WidgetSceneMarkerItem.h"

UWidgetSceneMarkerPanel::UWidgetSceneMarkerPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WidgetRefreshType = EWidgetRefreshType::Tick;
	MarkerCanvas = nullptr;
	MarkerItemClass = UWidgetSceneMarkerItem::StaticClass();
}

void UWidgetSceneMarkerPanel::OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams)
{
	ResolveMarkerCanvas();
	ResolveMapBackground();
	Super::OnInitialize(InOwner, InParams);
	OnRefresh();
}

void UWidgetSceneMarkerPanel::OnRefresh()
{
	Super::OnRefresh();
	if(!USceneModule::IsValid())
	{
		Markers.Reset();
		K2_OnMarkersUpdated(Markers);
		DestroyMarkerItems();
		return;
	}
	MapView = USceneModule::Get().GetMapView(MarkerChannel);
	ResolveMarkerCanvas();
	if(MapBackground)
	{
		MapBackground->SetMapView(MapView, MarkerChannel);
		MapBackground->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	Markers = USceneModule::Get().GetMarkerViews(MarkerChannel, GetMarkerViewLocation(), GetMarkerViewYaw());
	AddPlayerMarker();
	K2_OnMarkersUpdated(Markers);
	UpdateMarkerItems();
}

void UWidgetSceneMarkerPanel::OnDestroy(bool bRecovery)
{
	DestroyMarkerItems();
	Super::OnDestroy(bRecovery);
}

FVector UWidgetSceneMarkerPanel::GetMarkerViewLocation() const
{
	if(MarkerChannel == ESceneMarkerChannel::Map || MarkerChannel == ESceneMarkerChannel::MiniMap)
	{
		return FVector(MapView.Center, 0.f);
	}
	if(const APlayerController* PlayerController = UCommonModuleStatics::GetPlayerController())
	{
		if(const AActor* ViewTarget = PlayerController->GetViewTarget()) return ViewTarget->GetActorLocation();
	}
	return UCameraModuleStatics::GetCameraLocation(true);
}

float UWidgetSceneMarkerPanel::GetMarkerViewYaw() const
{
	if(MarkerChannel == ESceneMarkerChannel::Map || MarkerChannel == ESceneMarkerChannel::MiniMap) return MapView.Yaw;
	if(const APlayerController* PlayerController = UCommonModuleStatics::GetPlayerController())
	{
		return PlayerController->GetControlRotation().Yaw;
	}
	return 0.f;
}

bool UWidgetSceneMarkerPanel::ProjectMarker(const FSceneMarkerView& InMarker, FVector2D InPanelSize, FVector2D& OutPosition) const
{
	if(!USceneModule::IsValid()) return false;
	const USceneModule& Scene = USceneModule::Get();
	switch(MarkerChannel)
	{
		case ESceneMarkerChannel::Map:
			return USceneModuleStatics::ProjectMarkerToMap(InMarker, MapView.Center, MapView.Range, InPanelSize, MapView.Yaw, false, OutPosition);
		case ESceneMarkerChannel::MiniMap:
			return USceneModuleStatics::ProjectMarkerToMap(InMarker, MapView.Center, MapView.Range, InPanelSize, MapView.Yaw, true, OutPosition);
		case ESceneMarkerChannel::Compass:
		{
			float Position = 0.f;
			if(!USceneModuleStatics::ProjectMarkerToCompass(InMarker, 180.f, InPanelSize.X, false, Position)) return false;
			OutPosition = FVector2D(Position, InPanelSize.Y * 0.5f);
			return true;
		}
		default: return false;
	}
}

void UWidgetSceneMarkerPanel::UpdateMarkerItems()
{
	ResolveMarkerCanvas();
	if(!MarkerCanvas || !MarkerItemClass) return;
	const FVector2D PanelSize = MarkerCanvas->GetCachedGeometry().GetLocalSize();
	TSet<FGuid> DesiredIDs;
	for(const FSceneMarkerView& Marker : Markers)
	{
		if(!Marker.Marker.MarkerID.IsValid()) continue;
		DesiredIDs.Add(Marker.Marker.MarkerID);
		UWidgetSceneMarkerItem*& Item = MarkerItems.FindOrAdd(Marker.Marker.MarkerID);
		if(!Item)
		{
			Item = CreateSubWidget<UWidgetSceneMarkerItem>({}, MarkerItemClass);
			if(!Item)
			{
				MarkerItems.Remove(Marker.Marker.MarkerID);
				continue;
			}
			if(UCanvasPanelSlot* CanvasSlot = MarkerCanvas->AddChildToCanvas(Item))
			{
				CanvasSlot->SetAutoSize(true);
				CanvasSlot->SetAlignment(FVector2D(0.f, 0.5f));
			}
		}
		Item->SetMarkerView(Marker, bShowMarkerNames, bShowMarkerDistance);

		FVector2D Position;
		if(PanelSize.IsNearlyZero() || !ProjectMarker(Marker, PanelSize, Position))
		{
			Item->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		Item->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if(UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Item->Slot)) CanvasSlot->SetPosition(Position - FVector2D(UWidgetSceneMarkerItem::IconSize * 0.5f, 0.f));
	}
	TArray<FGuid> RemovedIDs;
	MarkerItems.GetKeys(RemovedIDs);
	for(const FGuid& MarkerID : RemovedIDs)
	{
		if(DesiredIDs.Contains(MarkerID)) continue;
		if(UWidgetSceneMarkerItem* Item = MarkerItems.FindRef(MarkerID)) DestroySubWidget(Item, true);
		MarkerItems.Remove(MarkerID);
	}
}

void UWidgetSceneMarkerPanel::ResolveMapBackground()
{
	if(!MarkerCanvas || !WidgetTree || !USceneModule::IsValid()) return;
	const USceneModule& Scene = USceneModule::Get();
	const bool bUsesWidget = Scene.GetMapBackgroundWidgetClass() &&
		(MarkerChannel == ESceneMarkerChannel::Map ||
			MarkerChannel == ESceneMarkerChannel::MiniMap && Scene.GetMiniMapSource() == EWorldMiniMapSource::Widget);
	if(!bUsesWidget)
	{
		if(MapBackground) MapBackground->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	if(!MapBackground || !MapBackground->IsA(Scene.GetMapBackgroundWidgetClass()))
	{
		if(MapBackground) MapBackground->RemoveFromParent();
		MapBackground = WidgetTree->ConstructWidget<UWidgetSceneMapBase>(Scene.GetMapBackgroundWidgetClass(), TEXT("MapBackground"));
		if(UCanvasPanelSlot* BackgroundSlot = MarkerCanvas->AddChildToCanvas(MapBackground))
		{
			BackgroundSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			BackgroundSlot->SetOffsets(FMargin(0.f));
			BackgroundSlot->SetZOrder(-1);
		}
		MapBackground->SetClipping(EWidgetClipping::ClipToBounds);
	}
}

void UWidgetSceneMarkerPanel::AddPlayerMarker()
{
	if(!bShowPlayerMarker || (MarkerChannel != ESceneMarkerChannel::Map && MarkerChannel != ESceneMarkerChannel::MiniMap)) return;
	const APawn* Player = UCommonModuleStatics::GetPlayerPawn();
	if(!Player) return;

	FSceneMarkerView View;
	View.Marker.MarkerID = FGuid(0x504C4159u, 0x45524D41u, 0x504D4152u, 0x4B455231u);
	View.Marker.DisplayName = NSLOCTEXT("SceneMarker", "Player", "玩家");
	View.Marker.Color = FLinearColor(0.1f, 0.8f, 1.f);
	View.Marker.Location = Player->GetActorLocation();
	View.Marker.Channels = static_cast<int32>(MarkerChannel);
	View.Marker.Priority = MAX_int32;
	View.Location = View.Marker.Location;
	View.Distance = FVector2D(View.Location - GetMarkerViewLocation()).Size();
	View.bTargetLoaded = true;
	Markers.Add(MoveTemp(View));
}

void UWidgetSceneMarkerPanel::ResolveMarkerCanvas()
{
	if(MarkerCanvas || !WidgetTree || MarkerCanvasName.IsNone()) return;
	MarkerCanvas = Cast<UCanvasPanel>(WidgetTree->FindWidget(MarkerCanvasName));
	if(!MarkerCanvas && !MarkerCanvasHostName.IsNone())
	{
		UWidget* MarkerHost = WidgetTree->FindWidget(MarkerCanvasHostName);
		if(Cast<UOverlay>(MarkerHost) || Cast<UCanvasPanel>(MarkerHost))
		{
			MarkerCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), MarkerCanvasName);
			if(UOverlay* MarkerOverlay = Cast<UOverlay>(MarkerHost))
			{
				if(UOverlaySlot* OverlaySlot = MarkerOverlay->AddChildToOverlay(MarkerCanvas))
				{
					OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
					OverlaySlot->SetVerticalAlignment(VAlign_Fill);
				}
			}
			else if(UCanvasPanel* HostCanvas = Cast<UCanvasPanel>(MarkerHost))
			{
				if(UCanvasPanelSlot* CanvasSlot = HostCanvas->AddChildToCanvas(MarkerCanvas))
				{
					CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
					CanvasSlot->SetOffsets(FMargin(0.f));
				}
			}
		}
	}
	if(MarkerCanvas)
	{
		MarkerCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void UWidgetSceneMarkerPanel::DestroyMarkerItems()
{
	for(const auto& Pair : MarkerItems)
	{
		if(Pair.Value) DestroySubWidget(Pair.Value, true);
	}
	MarkerItems.Reset();
}
