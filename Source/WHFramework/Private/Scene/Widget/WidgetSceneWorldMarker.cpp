#include "Scene/Widget/WidgetSceneWorldMarker.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Scene/Widget/WidgetSceneMarkerItem.h"

UWidgetSceneWorldMarker::UWidgetSceneWorldMarker(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WidgetRefreshType = EWidgetRefreshType::Procedure;
}

void UWidgetSceneWorldMarker::RefreshLocation_Implementation(UWidget* InWidget, FWorldWidgetMapping InMapping)
{
	Super::RefreshLocation_Implementation(InWidget, InMapping);
	if(UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(InWidget ? InWidget->Slot : nullptr))
	{
		CanvasSlot->SetPosition(CanvasSlot->GetPosition() - FVector2D(UWidgetSceneMarkerItem::IconSize * 0.5f, 0.f));
	}
}

TSharedRef<SWidget> UWidgetSceneWorldMarker::RebuildWidget()
{
	if(WidgetTree && !WidgetTree->RootWidget)
	{
		MarkerItem = WidgetTree->ConstructWidget<UWidgetSceneMarkerItem>(UWidgetSceneMarkerItem::StaticClass(), TEXT("MarkerItem"));
		WidgetTree->RootWidget = MarkerItem;
	}
	const TSharedRef<SWidget> Result = Super::RebuildWidget();
	if(MarkerItem) MarkerItem->SetMarkerView(MarkerView, true, true);
	return Result;
}

void UWidgetSceneWorldMarker::SetMarkerView(const FSceneMarkerView& InMarkerView)
{
	MarkerView = InMarkerView;
	BindWidgetMap.FindOrAdd(this) = FWorldWidgetMapping(MarkerView.Location);
	bMarkerInRange = MarkerView.Distance >= MarkerView.Marker.MinDistance &&
		(MarkerView.Marker.MaxDistance <= 0.f || MarkerView.Distance <= MarkerView.Marker.MaxDistance);
	if(MarkerItem) MarkerItem->SetMarkerView(MarkerView, true, true);
}

void UWidgetSceneWorldMarker::UpdateMarkerState(const FSceneMarkerView& InMarkerView)
{
	MarkerView.Location = InMarkerView.Location;
	MarkerView.Distance = InMarkerView.Distance;
	MarkerView.Bearing = InMarkerView.Bearing;
	MarkerView.bTargetLoaded = InMarkerView.bTargetLoaded;
	MarkerView.bTracked = InMarkerView.bTracked;
	BindWidgetMap.FindOrAdd(this) = FWorldWidgetMapping(MarkerView.Location);
	bMarkerInRange = MarkerView.Distance >= MarkerView.Marker.MinDistance &&
		(MarkerView.Marker.MaxDistance <= 0.f || MarkerView.Distance <= MarkerView.Marker.MaxDistance);
	if(MarkerItem) MarkerItem->UpdateMarkerState(MarkerView);
}

bool UWidgetSceneWorldMarker::IsWidgetVisible_Implementation(bool bRefresh)
{
	return bMarkerInRange && Super::IsWidgetVisible_Implementation(bRefresh);
}
