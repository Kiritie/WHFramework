#pragma once

#include "Engine/DeveloperSettings.h"
#include "WidgetThemeEditorSettings.generated.h"

UCLASS(Config = EditorPerProjectUserSettings, meta = (DisplayName = "Widget Theme Editor"))
class WHFRAMEWORKEDITOR_API UWidgetThemeEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Content Browser")
	bool bHideThemeFolders = true;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;

	static void ApplyFolderVisibility();

	static void ClearFolderVisibility();
};
