#include "Widget/Style/CommonProgressStyle.h"
#include "Components/ProgressBar.h"

UCommonProgressStyle::UCommonProgressStyle()
{
	ProgressBarStyle = GetDefault<UProgressBar>()->GetWidgetStyle();
	RadialBackground.DrawAs = ESlateBrushDrawType::NoDrawType;
}
