// Copyright Epic Games, Inc. All Rights Reserved.

#include "Task/Asset/TaskAssetDefinition.h"

#include "Task/TaskEditor.h"
#include "Task/Base/TaskAsset.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UTaskAssetDefinition::UTaskAssetDefinition()
{
	AssetDisplayName = NSLOCTEXT("AssetTypeActions", "AssetTypeActions_TaskAsset", "Task Asset");
	AssetColor = FLinearColor(FColor(62, 140, 35));
	AssetClass = UTaskAsset::StaticClass();
}

EAssetCommandResult UTaskAssetDefinition::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	FTaskEditorModule& TaskEditorModule = FTaskEditorModule::Get();

	for (UTaskAsset* Task : OpenArgs.LoadObjects<UTaskAsset>())
	{
		TaskEditorModule.CreateTaskEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Task);
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
