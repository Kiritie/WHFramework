#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Widget/Theme/WidgetStyleTypes.h"
class UWidgetTheme;
class WHFRAMEWORK_API FWidgetThemeResolver
{
public:
	static const UWidgetTheme* ResolveTheme(const UObject* Context);
	static UClass* ResolveClass(const UObject* Context, FGameplayTag Tag, EWidgetStyleKind Kind);
	static UClass* NativeBase(EWidgetStyleKind Kind);
	static UClass* DefaultStyle(EWidgetStyleKind Kind);
	static bool Validate(const UWidgetTheme* Theme, FString& Error);
	static FSimpleMulticastDelegate& OnThemeChanged();
	static void NotifyThemeChanged();
#if WITH_EDITOR

	static void SetPreviewOverride(const UObject* Root, UWidgetTheme* Theme);
	static void ClearPreviewOverride(const UObject* Root);
	static void InvalidateEditorDefault();
#endif
};
