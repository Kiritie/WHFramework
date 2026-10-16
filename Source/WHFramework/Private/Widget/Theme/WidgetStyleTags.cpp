#include "Widget/Theme/WidgetStyleTags.h"

namespace WidgetStyleTags
{
	UE_DEFINE_GAMEPLAY_TAG(Button, "Style.Button");
	UE_DEFINE_GAMEPLAY_TAG(Text, "Style.Text");
	UE_DEFINE_GAMEPLAY_TAG(Brush, "Style.Brush");
	UE_DEFINE_GAMEPLAY_TAG(Input, "Style.Input");
	UE_DEFINE_GAMEPLAY_TAG(Toggle, "Style.Toggle");
	UE_DEFINE_GAMEPLAY_TAG(Range, "Style.Range");
	UE_DEFINE_GAMEPLAY_TAG(Progress, "Style.Progress");
	UE_DEFINE_GAMEPLAY_TAG(Scroll, "Style.Scroll");
	UE_DEFINE_GAMEPLAY_TAG(Border, "Style.Border");
	UE_DEFINE_GAMEPLAY_TAG(TextScroll, "Style.TextScroll");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(ButtonDefault, "Style.Button.Common.Default");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TextDefault, "Style.Text.Common.Default");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(BrushDefault, "Style.Brush.Common.Default");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(InputDefault, "Style.Input.Common.Default");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(ToggleDefault, "Style.Toggle.Common.Default");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(RangeDefault, "Style.Range.Common.Default");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(ProgressDefault, "Style.Progress.Common.Default");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(ScrollDefault, "Style.Scroll.Common.Default");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(BorderDefault, "Style.Border.Common.Default");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TextScrollDefault, "Style.TextScroll.Common.Default");

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
