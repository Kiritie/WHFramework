#include "Scene/Widget/WidgetSceneMapBase.h"

void UWidgetSceneMapBase::SetMapView(const FSceneMapView& InMapView, ESceneMarkerChannel InMarkerChannel)
{
	MapView = InMapView;
	MarkerChannel = InMarkerChannel;
	InvalidateLayoutAndVolatility();
}
