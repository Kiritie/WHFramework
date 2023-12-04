// Copyright Epic Games, Inc. All Rights Reserved.

#include "Procedure/Asset/ProcedureAssetDefinition.h"

#include "Procedure/ProcedureEditor.h"
#include "Procedure/Base/ProcedureAsset.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UProcedureAssetDefinition::UProcedureAssetDefinition()
{
	AssetDisplayName = NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ProcedureAsset", "Procedure Asset");
	AssetColor = FLinearColor(FColor(62, 140, 35));
	AssetClass = UProcedureAsset::StaticClass();
}

EAssetCommandResult UProcedureAssetDefinition::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	FProcedureEditorModule& ProcedureEditorModule = FProcedureEditorModule::Get();

	for (UProcedureAsset* Procedure : OpenArgs.LoadObjects<UProcedureAsset>())
	{
		ProcedureEditorModule.CreateProcedureEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Procedure);
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
