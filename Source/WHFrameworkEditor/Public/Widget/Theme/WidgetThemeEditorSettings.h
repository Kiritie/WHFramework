#pragma once

#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "WidgetThemeEditorSettings.generated.h"

UCLASS(Config = EditorPerProjectUserSettings, meta = (DisplayName = "Widget Theme Editor"))
class WHFRAMEWORKEDITOR_API UWidgetThemeEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Content Browser")
	bool bHideExternalAssetFolders = true;

	UPROPERTY(Config, EditAnywhere, Category = "Tags", meta = (DisplayName = "Show UI.Style prefix"))
	bool bShowStyleTagPrefix = false;

	static FText GetStyleTagDisplayName(FGameplayTag Tag);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;

	static void ApplyFolderVisibility();

	static void ClearFolderVisibility();
};
