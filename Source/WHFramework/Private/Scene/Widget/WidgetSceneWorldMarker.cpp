#include "Scene/Widget/WidgetSceneWorldMarker.h"

#include "Blueprint/WidgetTree.h"
#include "Scene/Widget/WidgetSceneMarkerItem.h"

UWidgetSceneWorldMarker::UWidgetSceneWorldMarker(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WidgetName = TEXT("SceneWorldMarker");
	WidgetSpace = EWidgetSpace::Screen;
	WidgetZOrder = 2;
	bWidgetAutoSize = true;
	WidgetAlignment = FVector2D(0.5f, 1.f);
	WidgetRefreshType = EWidgetRefreshType::Procedure;
	WidgetVisibility = EWorldWidgetVisibility::ScreenOnly;
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
	if(MarkerItem) MarkerItem->SetMarkerView(MarkerView, true, true);
	RefreshLocationAndVisibility();
}
