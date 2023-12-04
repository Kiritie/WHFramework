// Copyright Epic Games, Inc. All Rights Reserved.

#include "Main/Blueprint/ModuleBlueprintActions.h"

#include "Main/MainEditor.h"
#include "Main/Base/ModuleBlueprint.h"
#include "Main/Blueprint/ModuleBlueprintFactory.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FModuleBlueprintActions::FModuleBlueprintActions(EAssetTypeCategories::Type InAssetCategory) : FBlueprintActionsBase(InAssetCategory)
{
	AssetName = NSLOCTEXT("AssetTypeActions", "ModuleBlueprintActions", "Module Blueprint");
	AssetColor = FColor(0, 96, 128);
	AssetClass = UModuleBlueprint::StaticClass();
	FactoryClass = UModuleBlueprintFactory::StaticClass();
}

TSharedRef<FBlueprintEditorBase> FModuleBlueprintActions::CreateBlueprintEditor() const
{
	return MakeShareable(new FModuleBlueprintEditor());
}

#undef LOCTEXT_NAMESPACE
