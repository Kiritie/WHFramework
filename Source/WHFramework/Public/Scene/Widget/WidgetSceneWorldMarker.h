#pragma once

#include "Scene/SceneModuleTypes.h"
#include "Widget/World/WorldWidgetBase.h"
#include "WidgetSceneWorldMarker.generated.h"

class UWidgetSceneMarkerItem;

/** A dedicated world-space task/scene marker rendered by the world-widget layer. */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UWidgetSceneWorldMarker : public UWorldWidgetBase
{
	GENERATED_BODY()

public:
	UWidgetSceneWorldMarker(const FObjectInitializer& ObjectInitializer);

	void SetMarkerView(const FSceneMarkerView& InMarkerView);
	FGuid GetMarkerID() const { return MarkerView.Marker.MarkerID; }
	const FSceneMarkerView& GetMarkerView() const { return MarkerView; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void RefreshLocation_Implementation(UWidget* InWidget, FWorldWidgetMapping InMapping) override;

	UPROPERTY(Transient)
	UWidgetSceneMarkerItem* MarkerItem = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Marker")
	FSceneMarkerView MarkerView;
};
