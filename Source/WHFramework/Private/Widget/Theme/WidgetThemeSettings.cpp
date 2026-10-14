#include "Widget/Theme/WidgetThemeSettings.h"
#include "Widget/Theme/WidgetThemeResolver.h"
#include "Widget/Style/SubWidgetStyle.h"

UWidgetThemeSettings::UWidgetThemeSettings()
{
	ButtonStyleBase = USubWidgetStyle::StaticClass();
	TextStyleBase = UCommonTextStyle::StaticClass();
	BrushStyleBase = UWidgetBrushStyle::StaticClass();
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
