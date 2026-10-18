#include "Widget/Theme/WidgetThemeEditorSettings.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/NamePermissionList.h"
#include "Misc/Paths.h"
#include "Widget/Theme/WidgetThemeEditorService.h"

FText UWidgetThemeEditorSettings::GetStyleTagDisplayName(FGameplayTag Tag)
{
	if (!Tag.IsValid())
	{
		return FText::GetEmpty();
	}
	FString Name = Tag.ToString();
	if (!GetDefault<UWidgetThemeEditorSettings>()->bShowStyleTagPrefix)
	{
		Name.RemoveFromStart(TEXT("UI.Style."), ESearchCase::CaseSensitive);
	}
	return FText::FromString(Name);
}

void UWidgetThemeEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	ApplyFolderVisibility();
}

void UWidgetThemeEditorSettings::ApplyFolderVisibility()
{
	ClearFolderVisibility();
	if (!GetDefault<UWidgetThemeEditorSettings>()->bHideExternalAssetFolders)
	{
		return;
	}
	auto& Visibility = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().GetFolderPermissionList();
	for (const FString& Root : FWidgetThemeEditorService::GetCreationRoots())
	{
		Visibility->AddDenyListItem(TEXT("WidgetThemeEditor"), FPaths::GetPath(Root));
	}
}

void UWidgetThemeEditorSettings::ClearFolderVisibility()
{
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get().GetFolderPermissionList()->UnregisterOwner(TEXT("WidgetThemeEditor"));
	}
}
