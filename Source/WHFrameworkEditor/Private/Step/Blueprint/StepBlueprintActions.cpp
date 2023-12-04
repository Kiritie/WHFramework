// Copyright Epic Games, Inc. All Rights Reserved.

#include "Step/Blueprint/StepBlueprintActions.h"

#include "Misc/MessageDialog.h"
#include "Step/StepEditor.h"
#include "Step/Base/StepBlueprint.h"
#include "Step/Blueprint/StepBlueprintFactory.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FStepBlueprintActions::FStepBlueprintActions(EAssetTypeCategories::Type InAssetCategory) : FBlueprintActionsBase(InAssetCategory)
{
	AssetName = NSLOCTEXT("AssetTypeActions", "StepBlueprintActions", "Step Blueprint");
	AssetColor = FColor(0, 96, 128);
	AssetClass = UStepBlueprint::StaticClass();
	FactoryClass = UStepBlueprintFactory::StaticClass();
}

TSharedRef<FBlueprintEditorBase> FStepBlueprintActions::CreateBlueprintEditor() const
{
	return MakeShareable(new FStepBlueprintEditor());
}

#undef LOCTEXT_NAMESPACE
