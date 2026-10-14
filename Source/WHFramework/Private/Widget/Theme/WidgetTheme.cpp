#include "Widget/Theme/WidgetTheme.h"
#include "Setting/Widget/Entry/WidgetSettingCategoryEntry.h"
#include "Setting/Widget/Entry/WidgetSettingEntryBase.h"

UWidgetTheme::UWidgetTheme()
{
	SettingCategoryEntryClass = UWidgetSettingCategoryEntry::StaticClass();
}

#if WITH_EDITOR
FWidgetThemeDuplicateDelegate UWidgetTheme::DuplicateStyles;

void UWidgetTheme::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	if (DuplicateMode == EDuplicateMode::Normal && !HasAnyFlags(RF_Transient | RF_ClassDefaultObject) && GetOutermost() != GetTransientPackage() &&
		!GetOutermost()->HasAnyFlags(RF_Transient) && !GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor | PKG_ForDiffing) && DuplicateStyles.IsBound())
	{
		bDuplicateFailed = !DuplicateStyles.Execute(this);
	}
}
#endif

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
	switch (InRenderer)
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
		for (const auto& Style : InStyles)
		{
			const FString Tag = Style.Key.ToString();
			if (!PreviousTag.IsEmpty() && Tag < PreviousTag)
			{
				return false;
			}
			PreviousTag = Tag;
		}
		return true;
	};

	return IsSorted(ButtonStyles) && IsSorted(TextStyles) && IsSorted(BrushStyles);
}

void UWidgetTheme::SortStyles()
{
	const auto CompareTags = [](const FGameplayTag& Left, const FGameplayTag& Right) { return Left.ToString() < Right.ToString(); };

	ButtonStyles.KeySort(CompareTags);
	TextStyles.KeySort(CompareTags);
	BrushStyles.KeySort(CompareTags);
}
#endif

UClass* UWidgetTheme::GetStyleClass(FGameplayTag Tag, EWidgetStyleKind Kind) const
{
	switch (Kind)
	{
	case EWidgetStyleKind::Button:
	{
		const auto* D = FindButtonStyle(Tag);
		return D ? D->Style.Get() : nullptr;
	}
	case EWidgetStyleKind::Text:
	{
		const auto* D = FindTextStyle(Tag);
		return D ? D->Style.Get() : nullptr;
	}
	case EWidgetStyleKind::Brush:
	{
		const auto* D = FindBrushStyle(Tag);
		return D ? D->Style.Get() : nullptr;
	}
	}
	return nullptr;
}
#if WITH_EDITOR
bool UWidgetTheme::SetStyleClass(FGameplayTag Tag, EWidgetStyleKind Kind, UClass* StyleClass)
{
	if (!Tag.IsValid() || !StyleClass || StyleClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return false;
	}

	if ((Kind != EWidgetStyleKind::Button && ButtonStyles.Contains(Tag)) || (Kind != EWidgetStyleKind::Text && TextStyles.Contains(Tag)) ||
		(Kind != EWidgetStyleKind::Brush && BrushStyles.Contains(Tag)))
	{
		return false;
	}
	switch (Kind)
	{
	case EWidgetStyleKind::Button:
		if (!StyleClass->IsChildOf(UCommonButtonStyle::StaticClass()))
		{
			return false;
		}
		ButtonStyles.FindOrAdd(Tag).Style = StyleClass;
		return true;
	case EWidgetStyleKind::Text:
		if (!StyleClass->IsChildOf(UCommonTextStyle::StaticClass()))
		{
			return false;
		}
		TextStyles.FindOrAdd(Tag).Style = StyleClass;
		return true;
	case EWidgetStyleKind::Brush:
		if (!StyleClass->IsChildOf(UWidgetBrushStyle::StaticClass()))
		{
			return false;
		}
		BrushStyles.FindOrAdd(Tag).Style = StyleClass;
		return true;
	}
	return false;
}
void UWidgetTheme::RemoveStyle(FGameplayTag Tag, EWidgetStyleKind Kind)
{
	switch (Kind)
	{
	case EWidgetStyleKind::Button:
		ButtonStyles.Remove(Tag);
		break;
	case EWidgetStyleKind::Text:
		TextStyles.Remove(Tag);
		break;
	case EWidgetStyleKind::Brush:
		BrushStyles.Remove(Tag);
		break;
	}
	if (!ButtonStyles.Contains(Tag) && !TextStyles.Contains(Tag) && !BrushStyles.Contains(Tag))
	{
		Bindings.Remove(Tag);
	}
}
#endif
