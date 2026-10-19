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
#include "Widget/Common/CommonRoundedClip.h"

UWidgetSceneMarkerPanel::UWidgetSceneMarkerPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WidgetRefreshType = EWidgetRefreshType::Tick;
	MarkerCanvas = nullptr;
	MarkerItemClass = UWidgetSceneMarkerItem::StaticClass();
}

void UWidgetSceneMarkerPanel::OnInitialize(const FParameter& InParams)
{
	const FScopedWorldContext WorldContext(GetWorld());
	if (MarkerScene.IsValid())
	{
		MarkerScene->OnSceneMarkersChanged.RemoveDynamic(this, &UWidgetSceneMarkerPanel::RefreshMarkerMembership);
		MarkerScene->OnMapPresentationChanged.Remove(MapPresentationChangedHandle);
	}
	MapPresentationChangedHandle.Reset();
	MarkerScene = USceneModule::GetPtr(GetWorld());
	if (MarkerScene.IsValid())
	{
		MarkerScene->OnSceneMarkersChanged.AddUniqueDynamic(this, &UWidgetSceneMarkerPanel::RefreshMarkerMembership);
		MapPresentationChangedHandle = MarkerScene->OnMapPresentationChanged.AddUObject(this, &UWidgetSceneMarkerPanel::ResolveMapBackground);
	}
	ResolveMarkerCanvas();
	ResolveMapContentCanvas();
	ResolveMapBackground();
	Super::OnInitialize(InParams);
	RefreshMarkerMembership();
	OnRefresh();
}

void UWidgetSceneMarkerPanel::OnRefresh()
{
	const FScopedWorldContext WorldContext(GetWorld());
	Super::OnRefresh();
	if(!USceneModule::IsValid())
	{
		MapView = FSceneMapView();
		bPlayerMarkerPresent = false;
		if(MapBackground) MapBackground->SetVisibility(ESlateVisibility::Collapsed);
		if(!Markers.IsEmpty() || !MarkerItems.IsEmpty())
		{
			Markers.Reset();
			K2_OnMarkersUpdated(Markers);
			DestroyMarkerItems();
		}
		return;
	}
	MapView = USceneModule::Get().GetMapView(MarkerChannel);
	ResolveMarkerCanvas();
	if(MapBackground)
	{
		MapBackground->SetMapView(MapView, MarkerChannel);
	}
	const bool bShouldHavePlayerMarker = bShowPlayerMarker &&
		(MarkerChannel == ESceneMarkerChannel::Map || MarkerChannel == ESceneMarkerChannel::MiniMap) && GetOwningPlayerPawn() != nullptr;
	if(bShouldHavePlayerMarker != bPlayerMarkerPresent) RefreshMarkerMembership();
	UpdateMarkerStates();
}

void UWidgetSceneMarkerPanel::OnDestroy(EObjectDespawnMode InMode)
{
	const FScopedWorldContext WorldContext(GetWorld());
	if (MarkerScene.IsValid())
	{
		MarkerScene->OnSceneMarkersChanged.RemoveDynamic(this, &UWidgetSceneMarkerPanel::RefreshMarkerMembership);
		MarkerScene->OnMapPresentationChanged.Remove(MapPresentationChangedHandle);
	}
	MapPresentationChangedHandle.Reset();
	MarkerScene.Reset();
	DestroyMarkerItems();
	Super::OnDestroy(InMode);
}

FVector UWidgetSceneMarkerPanel::GetMarkerViewLocation() const
{
	if(MarkerChannel == ESceneMarkerChannel::Map || MarkerChannel == ESceneMarkerChannel::MiniMap || MarkerChannel == ESceneMarkerChannel::Compass)
	{
		return FVector(MapView.Center, 0.f);
	}
	if(const APlayerController* PlayerController = GetOwningPlayer())
	{
		if(const AActor* ViewTarget = PlayerController->GetViewTarget()) return ViewTarget->GetActorLocation();
	}
	return UCameraModuleStatics::GetViewLocation();
}

float UWidgetSceneMarkerPanel::GetMarkerViewYaw() const
{
	if(MarkerChannel == ESceneMarkerChannel::Map || MarkerChannel == ESceneMarkerChannel::MiniMap || MarkerChannel == ESceneMarkerChannel::Compass) return MapView.Yaw;
	if(const APlayerController* PlayerController = GetOwningPlayer())
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
		{
			if (!MapContentCanvas || !MarkerCanvas) return false;
			const FGeometry& ContentGeometry = MapContentCanvas->GetCachedGeometry();
			FVector2D ContentPosition;
			if (!USceneModuleStatics::ProjectMarkerToMap(InMarker, MapView.Center, MapView.Range,
				ContentGeometry.GetLocalSize(), MapView.Yaw, true, ContentPosition)) return false;
			OutPosition = MarkerCanvas->GetCachedGeometry().AbsoluteToLocal(ContentGeometry.LocalToAbsolute(ContentPosition));
			return !OutPosition.ContainsNaN();
		}
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

void UWidgetSceneMarkerPanel::RefreshMarkerMembership()
{
	const FScopedWorldContext WorldContext(GetWorld());
	if(!USceneModule::IsValid())
	{
		Markers.Reset();
		bPlayerMarkerPresent = false;
		K2_OnMarkersUpdated(Markers);
		DestroyMarkerItems();
		return;
	}
	Markers = USceneModule::Get().GetMarkerViews(MarkerChannel, GetMarkerViewLocation(), GetMarkerViewYaw(), false);
	AddPlayerMarker();
	bPlayerMarkerPresent = bShowPlayerMarker &&
		(MarkerChannel == ESceneMarkerChannel::Map || MarkerChannel == ESceneMarkerChannel::MiniMap) && GetOwningPlayerPawn() != nullptr;
	K2_OnMarkersUpdated(Markers);
	RebuildMarkerItems();
}

void UWidgetSceneMarkerPanel::RebuildMarkerItems()
{
	ResolveMarkerCanvas();
	if(!MarkerCanvas || !MarkerItemClass) return;
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
		const FGameplayTag* StyleTag = MarkerStyleTags.Find(Marker.Marker.MarkerTag);
		Item->SetMarkerView(Marker, bShowMarkerNames, bShowMarkerDistance,
			Marker.bPlayer ? PlayerMarkerStyleTag : StyleTag ? *StyleTag : FGameplayTag());
	}
	TArray<FGuid> RemovedIDs;
	MarkerItems.GetKeys(RemovedIDs);
	for(const FGuid& MarkerID : RemovedIDs)
	{
		if(DesiredIDs.Contains(MarkerID)) continue;
		if(UWidgetSceneMarkerItem* Item = MarkerItems.FindRef(MarkerID)) DestroySubWidget(Item, EObjectDespawnMode::Recovery);
		MarkerItems.Remove(MarkerID);
	}
}

void UWidgetSceneMarkerPanel::UpdateMarkerStates()
{
	if(!USceneModule::IsValid() || !MarkerCanvas) return;
	const FVector ViewLocation = GetMarkerViewLocation();
	const float ViewYaw = GetMarkerViewYaw();
	const FVector2D PanelSize = MarkerCanvas->GetCachedGeometry().GetLocalSize();
	const APawn* Player = GetOwningPlayerPawn();
	for(FSceneMarkerView& Marker : Markers)
	{
		if(Marker.Marker.MarkerID == FGuid(0x504C4159u, 0x45524D41u, 0x504D4152u, 0x4B455231u))
		{
			if(Player)
			{
				Marker.Marker.Location = Player->GetActorLocation();
				Marker.Location = Marker.Marker.Location;
				Marker.Distance = FVector2D(Marker.Location - ViewLocation).Size();
				Marker.Bearing = FMath::FindDeltaAngleDegrees(ViewYaw, (Marker.Location - ViewLocation).Rotation().Yaw);
			}
		}
		else
		{
			Marker = USceneModule::Get().GetMarkerView(Marker.Marker.MarkerID, ViewLocation, ViewYaw);
		}
		UWidgetSceneMarkerItem* Item = MarkerItems.FindRef(Marker.Marker.MarkerID);
		if(!Item) continue;
		Item->UpdateMarkerState(Marker);
		if(Marker.bPlayer && Player)
		{
			Item->SetPlayerRotation(90.f - FMath::FindDeltaAngleDegrees(MapView.Yaw, Player->GetActorRotation().Yaw));
		}
		const bool bInRange = Marker.Distance >= Marker.Marker.MinDistance &&
			(Marker.Marker.MaxDistance <= 0.f || Marker.Distance <= Marker.Marker.MaxDistance);
		FVector2D Position;
		if(!bInRange || PanelSize.IsNearlyZero() || !ProjectMarker(Marker, PanelSize, Position))
		{
			Item->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		Item->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		const FVector2D HalfIconExtent = Item->GetIconExtent() * 0.5;
		if (MarkerChannel == ESceneMarkerChannel::MiniMap && MapContentCanvas == MarkerCanvas)
		{
			const FVector2D Inset(FMath::Min(HalfIconExtent.X, PanelSize.X * 0.5), FMath::Min(HalfIconExtent.Y, PanelSize.Y * 0.5));
			Position.X = FMath::Clamp(Position.X, Inset.X, PanelSize.X - Inset.X);
			Position.Y = FMath::Clamp(Position.Y, Inset.Y, PanelSize.Y - Inset.Y);
		}
		if(UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Item->Slot)) CanvasSlot->SetPosition(Position - FVector2D(HalfIconExtent.X, 0.f));
	}
}

void UWidgetSceneMarkerPanel::ResolveMapBackground()
{
	const FScopedWorldContext WorldContext(GetWorld());
	const USceneModule* Scene = MarkerScene.Get();
	const bool bUsesWidget = Scene && Scene->GetWorld() == GetWorld() && Scene->GetMapBackgroundWidgetClass() &&
		(MarkerChannel == ESceneMarkerChannel::Map ||
			MarkerChannel == ESceneMarkerChannel::MiniMap && Scene->GetMiniMapSource() == EWorldMiniMapSource::Widget && Scene->GetMiniMapMode() != EWorldMiniMapMode::None);
	if(!bUsesWidget)
	{
		if(MapBackground) MapBackground->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	ResolveMarkerCanvas();
	ResolveMapContentCanvas();
	if (!MapContentCanvas || !WidgetTree) return;
	const TSubclassOf<UWidgetSceneMapBase> DesiredClass = Scene->GetMapBackgroundWidgetClass();
	if(!MapBackground || MapBackground->GetClass() != DesiredClass.Get())
	{
		if(MapBackground) MapBackground->RemoveFromParent();
		MapBackground = WidgetTree->ConstructWidget<UWidgetSceneMapBase>(DesiredClass, MakeUniqueObjectName(WidgetTree, DesiredClass, TEXT("MapBackground")));
		if(UCanvasPanelSlot* BackgroundSlot = MapContentCanvas->AddChildToCanvas(MapBackground))
		{
			BackgroundSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			BackgroundSlot->SetOffsets(FMargin(0.f));
			BackgroundSlot->SetZOrder(MapBackgroundZOrder);
		}
		MapBackground->SetClipping(EWidgetClipping::ClipToBounds);
	}
	MapBackground->SetMapView(Scene->GetMapView(MarkerChannel), MarkerChannel);
	MapBackground->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UWidgetSceneMarkerPanel::AddPlayerMarker()
{
	if(!bShowPlayerMarker || (MarkerChannel != ESceneMarkerChannel::Map && MarkerChannel != ESceneMarkerChannel::MiniMap)) return;
	const APawn* Player = GetOwningPlayerPawn();
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
	View.bPlayer = true;
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

void UWidgetSceneMarkerPanel::ResolveMapContentCanvas()
{
	if (!MapContentCanvas)
	{
		MapContentCanvas = MapContentCanvasName.IsNone() ? MarkerCanvas :
			WidgetTree ? Cast<UCanvasPanel>(WidgetTree->FindWidget(MapContentCanvasName)) : nullptr;
	}
	if (!MapContentCanvas) return;
	MapContentCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (MapContentCornerRadius <= 0.f || MapContentCanvas == MarkerCanvas) return;
	if (!MapContentClip && WidgetTree)
	{
		UCanvasPanel* HostCanvas = Cast<UCanvasPanel>(MapContentCanvas->GetParent());
		UCanvasPanelSlot* ContentSlot = Cast<UCanvasPanelSlot>(MapContentCanvas->Slot);
		if (!HostCanvas || !ContentSlot) return;
		const FAnchorData Layout = ContentSlot->GetLayout();
		const int32 ZOrder = ContentSlot->GetZOrder();
		const bool bAutoSize = ContentSlot->GetAutoSize();
		MapContentClip = WidgetTree->ConstructWidget<UCommonRoundedClip>(UCommonRoundedClip::StaticClass(),
			MakeUniqueObjectName(WidgetTree, UCommonRoundedClip::StaticClass(), TEXT("MapContentClip")));
		MapContentCanvas->RemoveFromParent();
		MapContentClip->SetContent(MapContentCanvas);
		if (UCanvasPanelSlot* ClipSlot = HostCanvas->AddChildToCanvas(MapContentClip))
		{
			ClipSlot->SetLayout(Layout);
			ClipSlot->SetAutoSize(bAutoSize);
			ClipSlot->SetZOrder(ZOrder);
		}
	}
	if (MapContentClip) MapContentClip->SetCornerRadius(MapContentCornerRadius);
}

void UWidgetSceneMarkerPanel::DestroyMarkerItems()
{
	for(const auto& Pair : MarkerItems)
	{
		if(Pair.Value) DestroySubWidget(Pair.Value, EObjectDespawnMode::Recovery);
	}
	MarkerItems.Reset();
}
