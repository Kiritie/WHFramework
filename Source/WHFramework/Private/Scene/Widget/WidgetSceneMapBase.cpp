#include "Scene/Widget/WidgetSceneMapBase.h"
#include "Scene/SceneMapTypes.h"
#include "Scene/SceneMarkerTypes.h"

void UWidgetSceneMapBase::SetMapView(const FSceneMapView& InMapView, ESceneMarkerChannel InMarkerChannel)
{
	MapView = InMapView;
	MarkerChannel = InMarkerChannel;
	InvalidateLayoutAndVolatility();
}
