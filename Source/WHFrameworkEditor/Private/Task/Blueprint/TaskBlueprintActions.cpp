// Copyright Epic Games, Inc. All Rights Reserved.

#include "Task/Blueprint/TaskBlueprintActions.h"

#include "Misc/MessageDialog.h"
#include "Task/TaskEditor.h"
#include "Task/Base/TaskBlueprint.h"
#include "Task/Blueprint/TaskBlueprintFactory.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FTaskBlueprintActions::FTaskBlueprintActions(EAssetTypeCategories::Type InAssetCategory) : FBlueprintActionsBase(InAssetCategory)
{
	AssetName = NSLOCTEXT("AssetTypeActions", "TaskBlueprintActions", "Task Blueprint");
	AssetColor = FColor(0, 96, 128);
	AssetClass = UTaskBlueprint::StaticClass();
	FactoryClass = UTaskBlueprintFactory::StaticClass();
}

TSharedRef<FBlueprintEditorBase> FTaskBlueprintActions::CreateBlueprintEditor() const
{
	return MakeShareable(new FTaskBlueprintEditor());
}

#undef LOCTEXT_NAMESPACE
