// Copyright Epic Games, Inc. All Rights Reserved.

#include "Procedure/Blueprint/ProcedureBlueprintActions.h"

#include "Misc/MessageDialog.h"
#include "Procedure/ProcedureEditor.h"
#include "Procedure/Base/ProcedureBlueprint.h"
#include "Procedure/Blueprint/ProcedureBlueprintFactory.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FProcedureBlueprintActions::FProcedureBlueprintActions(EAssetTypeCategories::Type InAssetCategory) : FBlueprintActionsBase(InAssetCategory)
{
	AssetName = NSLOCTEXT("AssetTypeActions", "ProcedureBlueprintActions", "Procedure Blueprint");
	AssetColor = FColor(0, 96, 128);
	AssetClass = UProcedureBlueprint::StaticClass();
	FactoryClass = UProcedureBlueprintFactory::StaticClass();
}

TSharedRef<FBlueprintEditorBase> FProcedureBlueprintActions::CreateBlueprintEditor() const
{
	return MakeShareable(new FProcedureBlueprintEditor());
}

#undef LOCTEXT_NAMESPACE
