#include "Widget/Theme/WidgetTheme.h"
#include "Setting/Widget/Entry/WidgetSettingEntryBase.h"

UWidgetTheme::UWidgetTheme()
{
}

const FWidgetButtonStyleData* UWidgetTheme::FindButtonStyle(FGameplayTag InStyleTag) const
{
	return InStyleTag.IsValid() ? ButtonStyles.Find(InStyleTag) : nullptr;
}

const FWidgetTextStyleData* UWidgetTheme::FindTextStyle(FGameplayTag InStyleTag) const
{
	return InStyleTag.IsValid() ? TextStyles.Find(InStyleTag) : nullptr;
}

const FWidgetBrushStyleData* UWidgetTheme::FindBrushStyle(FGameplayTag InStyleTag) const
{
	return InStyleTag.IsValid() ? BrushStyles.Find(InStyleTag) : nullptr;
}

TSubclassOf<UWidgetSettingEntryBase> UWidgetTheme::FindSettingRendererClass(ESettingRendererType InRenderer) const
{
	switch(InRenderer)
	{
		case ESettingRendererType::Bool:
			return SettingRendererClasses.BoolClass;
		case ESettingRendererType::Number:
			return SettingRendererClasses.NumberClass;
		case ESettingRendererType::Enum:
			return SettingRendererClasses.EnumClass;
		case ESettingRendererType::Text:
			return SettingRendererClasses.TextClass;
		case ESettingRendererType::Option:
			return SettingRendererClasses.OptionClass;
		case ESettingRendererType::Key:
			return SettingRendererClasses.KeyClass;
		case ESettingRendererType::Custom:
			return SettingRendererClasses.CustomClass;
		default:
			return nullptr;
	}
}
