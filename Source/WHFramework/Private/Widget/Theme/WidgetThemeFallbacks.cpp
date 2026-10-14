#include "Widget/Theme/WidgetThemeFallbacks.h"
UWidgetFallbackButtonStyle::UWidgetFallbackButtonStyle()
{
	NormalTextStyle = UWidgetFallbackTextStyle::StaticClass();
	NormalHoveredTextStyle = NormalTextStyle;
	SelectedTextStyle = NormalTextStyle;
	SelectedHoveredTextStyle = NormalTextStyle;
	DisabledTextStyle = NormalTextStyle;
}
