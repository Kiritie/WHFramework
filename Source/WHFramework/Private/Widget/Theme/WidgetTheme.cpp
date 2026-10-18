#include "Widget/Theme/WidgetTheme.h"
#include "Widget/Theme/WidgetStyleTags.h"
#include "Widget/Theme/WidgetThemeResolver.h"
#include "Setting/Widget/Entry/WidgetSettingCategoryEntry.h"
#include "Setting/Widget/Entry/WidgetSettingEntryBase.h"

UWidgetTheme::UWidgetTheme()
{

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

void UWidgetTheme::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	FWidgetThemeResolver::NotifyThemeChanged();
}

void UWidgetTheme::PostEditUndo()
{
	Super::PostEditUndo();
	FWidgetThemeResolver::NotifyThemeChanged();
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
	TSet<const UWidgetTheme*> Visited;
	for (const UWidgetTheme* Theme = this; Theme && !Visited.Contains(Theme); Theme = Theme->ParentTheme)
	{
		Visited.Add(Theme);
		TSubclassOf<UWidgetSettingEntryBase> Renderer;
		switch (InRenderer)
		{
		case ESettingRendererType::Bool:
			Renderer = Theme->SettingRendererClasses.BoolClass;
			break;
		case ESettingRendererType::Number:
			Renderer = Theme->SettingRendererClasses.NumberClass;
			break;
		case ESettingRendererType::Enum:
			Renderer = Theme->SettingRendererClasses.EnumClass;
			break;
		case ESettingRendererType::Text:
			Renderer = Theme->SettingRendererClasses.TextClass;
			break;
		case ESettingRendererType::Option:
			Renderer = Theme->SettingRendererClasses.OptionClass;
			break;
		case ESettingRendererType::Key:
			Renderer = Theme->SettingRendererClasses.KeyClass;
			break;
		case ESettingRendererType::Custom:
			Renderer = Theme->SettingRendererClasses.CustomClass;
			break;
		default:
			return nullptr;
		}
		if (Renderer)
		{
			return Renderer;
		}
	}
	return nullptr;
}

TSubclassOf<UWidgetSettingCategoryEntry> UWidgetTheme::GetSettingCategoryEntryClass() const
{
	TSet<const UWidgetTheme*> Visited;
	for (const UWidgetTheme* Theme = this; Theme && !Visited.Contains(Theme); Theme = Theme->ParentTheme)
	{
		Visited.Add(Theme);
		if (Theme->SettingCategoryEntryClass)
		{
			return Theme->SettingCategoryEntryClass;
		}
	}
	return UWidgetSettingCategoryEntry::StaticClass();
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

	return IsSorted(ButtonStyles) && IsSorted(TextStyles) && IsSorted(BrushStyles) && IsSorted(InputStyles) && IsSorted(ToggleStyles) && IsSorted(RangeStyles) && IsSorted(ProgressStyles) && IsSorted(ScrollStyles) && IsSorted(BorderStyles) && IsSorted(TextScrollStyles);
}

void UWidgetTheme::SortStyles()
{
	const auto CompareTags = [](const FGameplayTag& Left, const FGameplayTag& Right) { return Left.ToString() < Right.ToString(); };

	ButtonStyles.KeySort(CompareTags);
	TextStyles.KeySort(CompareTags);
	BrushStyles.KeySort(CompareTags);
	InputStyles.KeySort(CompareTags);
	ToggleStyles.KeySort(CompareTags);
	RangeStyles.KeySort(CompareTags);
	ProgressStyles.KeySort(CompareTags);
	ScrollStyles.KeySort(CompareTags);
	BorderStyles.KeySort(CompareTags);
	TextScrollStyles.KeySort(CompareTags);

}
#endif

UClass* UWidgetTheme::GetStyleClass(FGameplayTag Tag, EWidgetStyleKind Kind) const
{
	switch (Kind)
	{
	case EWidgetStyleKind::Button:
	{
		const auto* Data = ButtonStyles.Find(Tag);
		return Data ? Data->Style.Get() : nullptr;
	}
	case EWidgetStyleKind::Text:
	{
		const auto* Data = TextStyles.Find(Tag);
		return Data ? Data->Style.Get() : nullptr;
	}
	case EWidgetStyleKind::Brush:
	{
		const auto* Data = BrushStyles.Find(Tag);
		return Data ? Data->Style.Get() : nullptr;
	}
	case EWidgetStyleKind::Input:
	{
		const auto* Data = InputStyles.Find(Tag);
		return Data ? Data->Style.Get() : nullptr;
	}
	case EWidgetStyleKind::Toggle:
	{
		const auto* Data = ToggleStyles.Find(Tag);
		return Data ? Data->Style.Get() : nullptr;
	}
	case EWidgetStyleKind::Range:
	{
		const auto* Data = RangeStyles.Find(Tag);
		return Data ? Data->Style.Get() : nullptr;
	}
	case EWidgetStyleKind::Progress:
	{
		const auto* Data = ProgressStyles.Find(Tag);
		return Data ? Data->Style.Get() : nullptr;
	}
	case EWidgetStyleKind::Scroll:
	{
		const auto* Data = ScrollStyles.Find(Tag);
		return Data ? Data->Style.Get() : nullptr;
	}
	case EWidgetStyleKind::Border:
	{
		const auto* Data = BorderStyles.Find(Tag);
		return Data ? Data->Style.Get() : nullptr;
	}
	case EWidgetStyleKind::TextScroll:
	{
		const auto* Data = TextScrollStyles.Find(Tag);
		return Data ? Data->Style.Get() : nullptr;
	}
	default:
		return nullptr;
	}
}

bool UWidgetTheme::HasLocalStyle(FGameplayTag Tag, EWidgetStyleKind Kind) const
{
	switch (Kind)
	{
	case EWidgetStyleKind::Button:
		return ButtonStyles.Contains(Tag);
	case EWidgetStyleKind::Text:
		return TextStyles.Contains(Tag);
	case EWidgetStyleKind::Brush:
		return BrushStyles.Contains(Tag);
	case EWidgetStyleKind::Input:
		return InputStyles.Contains(Tag);
	case EWidgetStyleKind::Toggle:
		return ToggleStyles.Contains(Tag);
	case EWidgetStyleKind::Range:
		return RangeStyles.Contains(Tag);
	case EWidgetStyleKind::Progress:
		return ProgressStyles.Contains(Tag);
	case EWidgetStyleKind::Scroll:
		return ScrollStyles.Contains(Tag);
	case EWidgetStyleKind::Border:
		return BorderStyles.Contains(Tag);
	case EWidgetStyleKind::TextScroll:
		return TextScrollStyles.Contains(Tag);
	default:
		return false;
	}
}

UClass* UWidgetTheme::ResolveStyleClass(FGameplayTag Tag, EWidgetStyleKind Kind) const
{
	TSet<const UWidgetTheme*> Visited;
	for (const UWidgetTheme* Theme = this; Theme && !Visited.Contains(Theme); Theme = Theme->ParentTheme)
	{
		Visited.Add(Theme);
		if (Theme->HasLocalStyle(Tag, Kind))
		{
			return Theme->GetStyleClass(Tag, Kind);
		}
	}
	return nullptr;
}

TArray<FGameplayTag> UWidgetTheme::GetStyleTags(EWidgetStyleKind Kind, bool bIncludeInherited) const
{
	TSet<FGameplayTag> Tags;
	TSet<const UWidgetTheme*> Visited;
	for (const UWidgetTheme* Theme = this; Theme && !Visited.Contains(Theme); Theme = bIncludeInherited ? Theme->ParentTheme.Get() : nullptr)
	{
		Visited.Add(Theme);
		switch (Kind)
		{
		case EWidgetStyleKind::Button:
			for (const auto& Entry : Theme->ButtonStyles)
			{
				Tags.Add(Entry.Key);
			}
			break;
		case EWidgetStyleKind::Text:
			for (const auto& Entry : Theme->TextStyles)
			{
				Tags.Add(Entry.Key);
			}
			break;
		case EWidgetStyleKind::Brush:
			for (const auto& Entry : Theme->BrushStyles)
			{
				Tags.Add(Entry.Key);
			}
			break;
		case EWidgetStyleKind::Input:
			for (const auto& Entry : Theme->InputStyles)
			{
				Tags.Add(Entry.Key);
			}
			break;
		case EWidgetStyleKind::Toggle:
			for (const auto& Entry : Theme->ToggleStyles)
			{
				Tags.Add(Entry.Key);
			}
			break;
		case EWidgetStyleKind::Range:
			for (const auto& Entry : Theme->RangeStyles)
			{
				Tags.Add(Entry.Key);
			}
			break;
		case EWidgetStyleKind::Progress:
			for (const auto& Entry : Theme->ProgressStyles)
			{
				Tags.Add(Entry.Key);
			}
			break;
		case EWidgetStyleKind::Scroll:
			for (const auto& Entry : Theme->ScrollStyles)
			{
				Tags.Add(Entry.Key);
			}
			break;
		case EWidgetStyleKind::Border:
			for (const auto& Entry : Theme->BorderStyles)
			{
				Tags.Add(Entry.Key);
			}
			break;
		case EWidgetStyleKind::TextScroll:
			for (const auto& Entry : Theme->TextScrollStyles)
			{
				Tags.Add(Entry.Key);
			}
			break;
		default:
			break;
		}
	}
	TArray<FGameplayTag> Result = Tags.Array();
	Result.Sort([](const FGameplayTag& Left, const FGameplayTag& Right) { return Left.ToString() < Right.ToString(); });
	return Result;
}

#if WITH_EDITOR
bool UWidgetTheme::SetStyleClass(FGameplayTag Tag, EWidgetStyleKind Kind, UClass* StyleClass)
{
	EWidgetStyleKind TagKind;
	if (!WidgetStyleTags::Kind(Tag, TagKind) || TagKind != Kind || WidgetStyleTags::IsRoot(Tag) || !StyleClass ||
		StyleClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return false;
	}
	switch (Kind)
	{
	case EWidgetStyleKind::Button:
		if (!StyleClass->IsChildOf(UCommonButtonStyleN::StaticClass()))
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
		if (!StyleClass->IsChildOf(UCommonBrushStyle::StaticClass()))
		{
			return false;
		}
		BrushStyles.FindOrAdd(Tag).Style = StyleClass;
		return true;
	case EWidgetStyleKind::Input:
		if (!StyleClass->IsChildOf(UCommonInputStyle::StaticClass()))
		{
			return false;
		}
		InputStyles.FindOrAdd(Tag).Style = StyleClass;
		return true;
	case EWidgetStyleKind::Toggle:
		if (!StyleClass->IsChildOf(UCommonToggleStyle::StaticClass()))
		{
			return false;
		}
		ToggleStyles.FindOrAdd(Tag).Style = StyleClass;
		return true;
	case EWidgetStyleKind::Range:
		if (!StyleClass->IsChildOf(UCommonRangeStyle::StaticClass()))
		{
			return false;
		}
		RangeStyles.FindOrAdd(Tag).Style = StyleClass;
		return true;
	case EWidgetStyleKind::Progress:
		if (!StyleClass->IsChildOf(UCommonProgressStyle::StaticClass()))
		{
			return false;
		}
		ProgressStyles.FindOrAdd(Tag).Style = StyleClass;
		return true;
	case EWidgetStyleKind::Scroll:
		if (!StyleClass->IsChildOf(UCommonScrollStyle::StaticClass()))
		{
			return false;
		}
		ScrollStyles.FindOrAdd(Tag).Style = StyleClass;
		return true;
	case EWidgetStyleKind::Border:
		if (!StyleClass->IsChildOf(UCommonBorderStyle::StaticClass()))
		{
			return false;
		}
		BorderStyles.FindOrAdd(Tag).Style = StyleClass;
		return true;
	case EWidgetStyleKind::TextScroll:
		if (!StyleClass->IsChildOf(UCommonTextScrollStyle::StaticClass()))
		{
			return false;
		}
		TextScrollStyles.FindOrAdd(Tag).Style = StyleClass;
		return true;
	default:
		return false;
	}
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
	case EWidgetStyleKind::Input:
		InputStyles.Remove(Tag);
		break;
	case EWidgetStyleKind::Toggle:
		ToggleStyles.Remove(Tag);
		break;
	case EWidgetStyleKind::Range:
		RangeStyles.Remove(Tag);
		break;
	case EWidgetStyleKind::Progress:
		ProgressStyles.Remove(Tag);
		break;
	case EWidgetStyleKind::Scroll:
		ScrollStyles.Remove(Tag);
		break;
	case EWidgetStyleKind::Border:
		BorderStyles.Remove(Tag);
		break;
	case EWidgetStyleKind::TextScroll:
		TextScrollStyles.Remove(Tag);
		break;
	default:
		return;
	}
	Bindings.Remove(Tag);
}
#endif
