#pragma once

#include "NativeGameplayTags.h"
#include "Widget/Theme/WidgetStyleTypes.h"

namespace WidgetStyleTags
{

	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Button);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Text);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Brush);

	WHFRAMEWORK_API FGameplayTag Root(EWidgetStyleKind Kind);
	WHFRAMEWORK_API bool Kind(FGameplayTag Tag, EWidgetStyleKind& OutKind);
	WHFRAMEWORK_API bool IsRoot(FGameplayTag Tag);

}
