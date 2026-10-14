#include "Widget/Theme/WidgetStyleTags.h"

namespace WidgetStyleTags
{

	UE_DEFINE_GAMEPLAY_TAG(Button, "Style.Button");
	UE_DEFINE_GAMEPLAY_TAG(Text, "Style.Text");
	UE_DEFINE_GAMEPLAY_TAG(Brush, "Style.Brush");

	FGameplayTag Root(EWidgetStyleKind Kind)
	{
		return Kind == EWidgetStyleKind::Button ? Button.GetTag() : Kind == EWidgetStyleKind::Text ? Text.GetTag() : Brush.GetTag();
	}

	bool Kind(FGameplayTag Tag, EWidgetStyleKind& OutKind)
	{
		for (EWidgetStyleKind Type : {EWidgetStyleKind::Button, EWidgetStyleKind::Text, EWidgetStyleKind::Brush})
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
		return Tag == Button || Tag == Text || Tag == Brush;
	}

}
