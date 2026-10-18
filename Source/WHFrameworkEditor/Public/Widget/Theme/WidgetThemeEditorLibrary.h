#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Widget/Theme/WidgetTheme.h"
#include "WidgetThemeEditorLibrary.generated.h"

class UBlueprint;

UCLASS()
class WHFRAMEWORKEDITOR_API UWidgetThemeEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "WidgetModule")
	static UWidgetTheme* CreateTheme(const FString& InAssetPath, UWidgetTheme* InParentTheme, bool bRuntimeTheme, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "WidgetModule")
	static UBlueprint* CreateStyle(UWidgetTheme* InTheme, const FString& InTag, UClass* InParentClass, const FString& InDestinationRoot, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "WidgetModule")
	static bool SetStyleProperty(UWidgetTheme* InTheme, FGameplayTag InTag, FName InProperty, const FString& InValue, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "WidgetModule")
	static bool SaveTheme(UWidgetTheme* InTheme, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "WidgetModule")
	static bool RemoveStyleBinding(UWidgetTheme* InTheme, const FString& InTag, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "WidgetModule")
	static bool RemoveDuplicateGlobalTagDefinition(const FString& InTag, const FString& InTargetOwnerPath, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "WidgetModule")
	static bool MigrateTag(const FString& InOldTag, const FString& InNewTag, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "WidgetModule")
	static bool MigrateTagReferences(const FString& InOldTagName, const FString& InNewTag, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "WidgetModule")
	static bool MigrateTagAlias(const FString& InOldAlias, const FString& InNewTag, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "WidgetModule")
	static bool SetProjectDefault(UWidgetTheme* InTheme, FString& OutError);
};
