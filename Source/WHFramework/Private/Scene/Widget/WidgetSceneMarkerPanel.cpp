#include "Scene/Widget/WidgetSceneMarkerPanel.h"

#include "Camera/CameraModuleStatics.h"
#include "Common/CommonModuleStatics.h"
#include "GameFramework/PlayerController.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Blueprint/WidgetTree.h"
#include "Scene/SceneModule.h"
#include "Scene/Capture/MiniMapCapture.h"
#include "Scene/SceneModuleStatics.h"
#include "Scene/Widget/WidgetSceneMarkerItem.h"

UWidgetSceneMarkerPanel::UWidgetSceneMarkerPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWidgetTickAble = true;
	WidgetRefreshType = EWidgetRefreshType::Tick;
	MarkerCanvas = nullptr;
	MarkerItemClass = UWidgetSceneMarkerItem::StaticClass();
}

void UWidgetSceneMarkerPanel::OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnInitialize(InOwner, InParams);
	ResolveMarkerCanvas();
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
	Markers = USceneModule::Get().GetMarkerViews(MarkerChannel, GetMarkerViewLocation(), GetMarkerViewYaw());
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
	if(MarkerChannel == ESceneMarkerChannel::Map && USceneModule::IsValid())
	{
		return FVector(USceneModule::Get().GetWorldMapCenter(), 0.f);
	}
	if(MarkerChannel == ESceneMarkerChannel::MiniMap && USceneModule::IsValid() && USceneModule::Get().GetMiniMapCapture())
	{
		return USceneModule::Get().GetMiniMapCapture()->GetActorLocation();
	}
	if(const APlayerController* PlayerController = UCommonModuleStatics::GetPlayerController())
	{
		if(const AActor* ViewTarget = PlayerController->GetViewTarget()) return ViewTarget->GetActorLocation();
	}
	return UCameraModuleStatics::GetCameraLocation(true);
}

float UWidgetSceneMarkerPanel::GetMarkerViewYaw() const
{
	if(MarkerChannel == ESceneMarkerChannel::Map) return 0.f;
	if(MarkerChannel == ESceneMarkerChannel::MiniMap && USceneModule::IsValid() && USceneModule::Get().GetMiniMapCapture())
	{
		return USceneModule::Get().GetMiniMapCapture()->GetActorRotation().Yaw;
	}
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
			return USceneModuleStatics::ProjectMarkerToMap(InMarker, Scene.GetWorldMapCenter(), Scene.GetWorldMapRange(), InPanelSize, 0.f, false, OutPosition);
		case ESceneMarkerChannel::MiniMap:
			return USceneModuleStatics::ProjectMarkerToMap(InMarker, FVector2D(GetMarkerViewLocation()), Scene.GetMiniMapRange(), InPanelSize, GetMarkerViewYaw(), true, OutPosition);
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
	if(PanelSize.IsNearlyZero()) return;
	TSet<FGuid> DesiredIDs;
	for(const FSceneMarkerView& Marker : Markers)
	{
		FVector2D Position;
		if(!Marker.Marker.MarkerID.IsValid() || !ProjectMarker(Marker, PanelSize, Position)) continue;
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
				CanvasSlot->SetAlignment(FVector2D(0.5f));
			}
		}
		Item->SetMarkerView(Marker, bShowMarkerNames, bShowMarkerDistance);
		Item->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if(UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Item->Slot)) CanvasSlot->SetPosition(Position);
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
		MarkerCanvas->SetClipping(EWidgetClipping::ClipToBounds);
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
