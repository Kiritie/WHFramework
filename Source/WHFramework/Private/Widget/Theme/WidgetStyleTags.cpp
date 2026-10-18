#include "Widget/Theme/WidgetStyleTags.h"

namespace WidgetStyleTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Button, "UI.Style.Button", "Root for button styles.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Text, "UI.Style.Text", "Root for text styles.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Brush, "UI.Style.Brush", "Root for brush and image styles.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input, "UI.Style.Input", "Root for editable input styles.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Toggle, "UI.Style.Toggle", "Root for toggle styles.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Range, "UI.Style.Range", "Root for numeric range and slider styles.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Progress, "UI.Style.Progress", "Root for progress indicator styles.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Scroll, "UI.Style.Scroll", "Root for scroll container styles.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Border, "UI.Style.Border", "Root for common border styles.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TextScroll, "UI.Style.TextScroll", "Root for scrolling text styles.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(ButtonDefault, "UI.Style.Button.Default", "Fallback button style when no specialized style is configured.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TextDefault, "UI.Style.Text.Default", "Fallback text style when no specialized style is configured.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BrushDefault, "UI.Style.Brush.Default", "Fallback brush style when no specialized style is configured.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputDefault, "UI.Style.Input.Default", "Fallback editable input style.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(ToggleDefault, "UI.Style.Toggle.Default", "Fallback toggle style.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(RangeDefault, "UI.Style.Range.Default", "Fallback numeric range style.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(ProgressDefault, "UI.Style.Progress.Default", "Fallback progress indicator style.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(ScrollDefault, "UI.Style.Scroll.Default", "Fallback scroll container style.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BorderDefault, "UI.Style.Border.Default", "Fallback border style.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TextScrollDefault, "UI.Style.TextScroll.Default", "Fallback scrolling text style.");

	TConstArrayView<EWidgetStyleKind> Kinds()
	{
		static constexpr EWidgetStyleKind Values[] =
		{
			EWidgetStyleKind::Button,
			EWidgetStyleKind::Text,
			EWidgetStyleKind::Brush,
			EWidgetStyleKind::Input,
			EWidgetStyleKind::Toggle,
			EWidgetStyleKind::Range,
			EWidgetStyleKind::Progress,
			EWidgetStyleKind::Scroll,
			EWidgetStyleKind::Border,
			EWidgetStyleKind::TextScroll,
		};
		return MakeArrayView(Values);
	}

	FString Label(EWidgetStyleKind InKind)
	{
		return InKind == EWidgetStyleKind::None ? TEXT("All") : StaticEnum<EWidgetStyleKind>()->GetNameStringByValue(static_cast<int64>(InKind));
	}

	FGameplayTag Root(EWidgetStyleKind InKind)
	{
		switch (InKind)
		{
		case EWidgetStyleKind::Button:
			return Button.GetTag();
		case EWidgetStyleKind::Text:
			return Text.GetTag();
		case EWidgetStyleKind::Brush:
			return Brush.GetTag();
		case EWidgetStyleKind::Input:
			return Input.GetTag();
		case EWidgetStyleKind::Toggle:
			return Toggle.GetTag();
		case EWidgetStyleKind::Range:
			return Range.GetTag();
		case EWidgetStyleKind::Progress:
			return Progress.GetTag();
		case EWidgetStyleKind::Scroll:
			return Scroll.GetTag();
		case EWidgetStyleKind::Border:
			return Border.GetTag();
		case EWidgetStyleKind::TextScroll:
			return TextScroll.GetTag();
		default:
			return FGameplayTag();
		}
	}

	FGameplayTag CommonDefault(EWidgetStyleKind InKind)
	{
		switch (InKind)
		{
		case EWidgetStyleKind::Button:
			return ButtonDefault.GetTag();
		case EWidgetStyleKind::Text:
			return TextDefault.GetTag();
		case EWidgetStyleKind::Brush:
			return BrushDefault.GetTag();
		case EWidgetStyleKind::Input:
			return InputDefault.GetTag();
		case EWidgetStyleKind::Toggle:
			return ToggleDefault.GetTag();
		case EWidgetStyleKind::Range:
			return RangeDefault.GetTag();
		case EWidgetStyleKind::Progress:
			return ProgressDefault.GetTag();
		case EWidgetStyleKind::Scroll:
			return ScrollDefault.GetTag();
		case EWidgetStyleKind::Border:
			return BorderDefault.GetTag();
		case EWidgetStyleKind::TextScroll:
			return TextScrollDefault.GetTag();
		default:
			return FGameplayTag();
		}
	}

	bool Kind(FGameplayTag Tag, EWidgetStyleKind& OutKind)
	{
		OutKind = EWidgetStyleKind::None;
		for (EWidgetStyleKind Type : Kinds())
		{
			if (Tag.IsValid() && Tag.MatchesTag(Root(Type)))
			{
				OutKind = Type;
				return true;
			}
		}
		return false;
	}

	bool IsRoot(FGameplayTag Tag)
	{
		EWidgetStyleKind Type;
		return Kind(Tag, Type) && Tag == Root(Type);
	}
}
