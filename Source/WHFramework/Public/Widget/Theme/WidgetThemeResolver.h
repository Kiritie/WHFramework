#pragma once
#include "CoreMinimal.h"
#include "Widget/Theme/WidgetStyleCatalog.h"
class UWidgetTheme;
class WHFRAMEWORK_API FWidgetThemeResolver
{
public:
	static const UWidgetTheme* ResolveTheme(const UObject* Context);
	static UClass* ResolveClass(const UObject* Context, FGameplayTag Tag, EWidgetStyleKind Kind);
	static UClass* NativeBase(EWidgetStyleKind Kind);
	static UClass* Placeholder(EWidgetStyleKind Kind);
	static bool Validate(const UWidgetTheme* Theme, const UWidgetStyleCatalog* Catalog, FString& Error, bool bRequireComplete = true);
	static FSimpleMulticastDelegate& OnThemeChanged();
	static void NotifyThemeChanged();
#if WITH_EDITOR

	static void SetPreviewOverride(const UObject* Root, UWidgetTheme* Theme);
	static void ClearPreviewOverride(const UObject* Root);
	static void InvalidateEditorDefault();
#endif
};
