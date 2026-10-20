#include "Widget/Style/CommonWindowStyle.h"
#include "Widget/Theme/WidgetStyleTags.h"

UCommonWindowStyle::UCommonWindowStyle()
{
	Background.DrawAs = ESlateBrushDrawType::RoundedBox;
	Background.TintColor = FSlateColor(FLinearColor(.06f, .06f, .06f, 1.f));
	Background.OutlineSettings.CornerRadii = FVector4(4.f, 4.f, 4.f, 4.f);
	Background.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	TitleTextStyle = UCommonTextStyle::StaticClass();
	CloseButtonStyleTag = WidgetStyleTags::CommonDefault(EWidgetStyleKind::Button);
	CloseButtonText = NSLOCTEXT("WH.Window", "Close", "关闭");
}
