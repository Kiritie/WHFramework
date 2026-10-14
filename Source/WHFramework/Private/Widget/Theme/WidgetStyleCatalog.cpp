#include "Widget/Theme/WidgetStyleCatalog.h"
#include "Widget/Style/SubWidgetStyle.h"
#include "Widget/Style/WidgetBrushStyle.h"
#include "CommonTextBlock.h"
UWidgetStyleCatalog::UWidgetStyleCatalog()
{
	NativeRoots.Add(EWidgetStyleKind::Button, USubWidgetStyle::StaticClass());
	NativeRoots.Add(EWidgetStyleKind::Text, UCommonTextStyle::StaticClass());
	NativeRoots.Add(EWidgetStyleKind::Brush, UWidgetBrushStyle::StaticClass());
}
