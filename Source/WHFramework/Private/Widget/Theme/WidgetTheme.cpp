#include "Widget/Theme/WidgetTheme.h"
#include "Setting/Widget/Entry/WidgetSettingCategoryEntry.h"
#include "Setting/Widget/Entry/WidgetSettingEntryBase.h"

UWidgetTheme::UWidgetTheme()
{
	SettingCategoryEntryClass = UWidgetSettingCategoryEntry::StaticClass();
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

#if WITH_EDITOR
bool UWidgetTheme::AreStylesSorted() const
{
	const auto IsSorted = [](const auto& InStyles)
	{
		FString PreviousTag;
		for(const auto& Style : InStyles)
		{
			const FString Tag = Style.Key.ToString();
			if(!PreviousTag.IsEmpty() && Tag < PreviousTag)
			{
				return false;
			}
			PreviousTag = Tag;
		}
		return true;
	};

	return IsSorted(ButtonStyles)
		&& IsSorted(TextStyles)
		&& IsSorted(BrushStyles);
}

void UWidgetTheme::SortStyles()
{
	const auto CompareTags = [](const FGameplayTag& Left, const FGameplayTag& Right)
	{
		return Left.ToString() < Right.ToString();
	};

	ButtonStyles.KeySort(CompareTags);
	TextStyles.KeySort(CompareTags);
	BrushStyles.KeySort(CompareTags);
}
#endif
