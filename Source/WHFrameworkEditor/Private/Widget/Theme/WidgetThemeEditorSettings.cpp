#include "Widget/Theme/WidgetThemeEditorSettings.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/NamePermissionList.h"
#include "Widget/Theme/WidgetThemeSettings.h"
#include "Widget/Theme/WidgetThemeEditorService.h"

void UWidgetThemeEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	ApplyFolderVisibility();
}

void UWidgetThemeEditorSettings::ApplyFolderVisibility()
{
	ClearFolderVisibility();
	if (!GetDefault<UWidgetThemeEditorSettings>()->bHideThemeFolders)
	{
		return;
	}
	auto& Visibility = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().GetFolderPermissionList();
	for (const FString& Root : FWidgetThemeEditorService::GetCreationRoots())
	{
		Visibility->AddDenyListItem(TEXT("WidgetThemeEditor"), Root);
	}
}

void UWidgetThemeEditorSettings::ClearFolderVisibility()
{
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get().GetFolderPermissionList()->UnregisterOwner(TEXT("WidgetThemeEditor"));
	}
}
