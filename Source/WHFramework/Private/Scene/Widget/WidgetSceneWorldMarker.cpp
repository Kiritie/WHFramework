#include "Scene/Widget/WidgetSceneWorldMarker.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Scene/Widget/WidgetSceneMarkerItem.h"

UWidgetSceneWorldMarker::UWidgetSceneWorldMarker(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WidgetName = TEXT("SceneWorldMarker");
	WidgetSpace = EWidgetSpace::Screen;
	WidgetZOrder = 2;
	bWidgetAutoSize = true;
	WidgetAlignment = FVector2D(0.f, 0.5f);
	WidgetRefreshType = EWidgetRefreshType::Procedure;
	WidgetVisibility = EWorldWidgetVisibility::ScreenOnly;
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
	if(MarkerItem) MarkerItem->SetMarkerView(MarkerView, true, true);
	RefreshLocationAndVisibility();
}
