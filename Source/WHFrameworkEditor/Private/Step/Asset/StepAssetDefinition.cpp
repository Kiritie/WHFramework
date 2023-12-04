// Copyright Epic Games, Inc. All Rights Reserved.

#include "Step/Asset/StepAssetDefinition.h"

#include "Step/StepEditor.h"
#include "Step/Base/StepAsset.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UStepAssetDefinition::UStepAssetDefinition()
{
	AssetDisplayName = NSLOCTEXT("AssetTypeActions", "AssetTypeActions_StepAsset", "Step Asset");
	AssetColor = FLinearColor(FColor(62, 140, 35));
	AssetClass = UStepAsset::StaticClass();
}

EAssetCommandResult UStepAssetDefinition::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	FStepEditorModule& StepEditorModule = FStepEditorModule::Get();

	for (UStepAsset* Step : OpenArgs.LoadObjects<UStepAsset>())
	{
		StepEditorModule.CreateStepEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Step);
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
