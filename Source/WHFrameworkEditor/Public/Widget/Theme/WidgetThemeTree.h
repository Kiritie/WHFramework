#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Widget/Theme/WidgetStyleCatalog.h"

class UWidgetTheme;

struct FThemeTreeItem
{
	FGameplayTag Tag;
	FString Tooltip;
	bool bRoot = false;
	bool bShared = false;
	TArray<TSharedPtr<FThemeTreeItem>> Children;
};

class WHFRAMEWORKEDITOR_API FWidgetThemeTree
{
public:
	static TArray<TSharedPtr<FThemeTreeItem>> Build(UWidgetTheme* Theme, EWidgetStyleKind TypeFilter, const FString& Search);
};
