#include "Widget/Theme/WidgetThemeSettings.h"
#include "Widget/Theme/WidgetThemeResolver.h"
#include "Widget/Style/CommonButtonStyleN.h"

UWidgetThemeSettings::UWidgetThemeSettings()
{
	ButtonStyleBase = UCommonButtonStyleN::StaticClass();
	TextStyleBase = UCommonTextStyle::StaticClass();
	BrushStyleBase = UCommonBrushStyle::StaticClass();
	InputStyleBase = UCommonInputStyle::StaticClass();
	ToggleStyleBase = UCommonToggleStyle::StaticClass();
	RangeStyleBase = UCommonRangeStyle::StaticClass();
	ProgressStyleBase = UCommonProgressStyle::StaticClass();
	ScrollStyleBase = UCommonScrollStyle::StaticClass();
	BorderStyleBase = UCommonBorderStyle::StaticClass();
	TextScrollStyleBase = UCommonTextScrollStyle::StaticClass();
	WindowStyleBase = UCommonWindowStyle::StaticClass();

}

UClass* UWidgetThemeSettings::GetStyleBase(EWidgetStyleKind Kind) const
{
	switch (Kind)
	{
	case EWidgetStyleKind::Button:
		return ButtonStyleBase.Get();
	case EWidgetStyleKind::Text:
		return TextStyleBase.Get();
	case EWidgetStyleKind::Brush:
		return BrushStyleBase.Get();
	case EWidgetStyleKind::Input:
		return InputStyleBase.Get();
	case EWidgetStyleKind::Toggle:
		return ToggleStyleBase.Get();
	case EWidgetStyleKind::Range:
		return RangeStyleBase.Get();
	case EWidgetStyleKind::Progress:
		return ProgressStyleBase.Get();
	case EWidgetStyleKind::Scroll:
		return ScrollStyleBase.Get();
	case EWidgetStyleKind::Border:
		return BorderStyleBase.Get();
	case EWidgetStyleKind::TextScroll:
		return TextScrollStyleBase.Get();
	case EWidgetStyleKind::Window:
		return WindowStyleBase.Get();
	default:
		return nullptr;
	}
}

#if WITH_EDITOR
void UWidgetThemeSettings::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	FWidgetThemeResolver::InvalidateEditorDefault();
}
#endif
