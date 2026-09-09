#pragma once

#include "Blueprint/UserWidget.h"
#include "Scene/SceneModuleTypes.h"
#include "WidgetSceneMapBase.generated.h"

UCLASS(Abstract, BlueprintType)
class WHFRAMEWORK_API UWidgetSceneMapBase : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetMapView(const FSceneMapView& InMapView, ESceneMarkerChannel InMarkerChannel);

	const FSceneMapView& GetMapView() const { return MapView; }
	ESceneMarkerChannel GetMarkerChannel() const { return MarkerChannel; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FSceneMapView MapView;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	ESceneMarkerChannel MarkerChannel = ESceneMarkerChannel::None;
};
