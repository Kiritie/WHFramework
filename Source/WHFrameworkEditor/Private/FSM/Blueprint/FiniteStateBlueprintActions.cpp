// Copyright Epic Games, Inc. All Rights Reserved.

#include "FSM/Blueprint/FiniteStateBlueprintActions.h"

#include "FSM/FSMEditor.h"
#include "FSM/Base/FiniteStateBlueprint.h"
#include "FSM/Blueprint/FiniteStateBlueprintFactory.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FFiniteStateBlueprintActions::FFiniteStateBlueprintActions(EAssetTypeCategories::Type InAssetCategory) : FBlueprintActionsBase(InAssetCategory)
{
	AssetName = NSLOCTEXT("AssetTypeActions", "FiniteStateBlueprintActions", "FiniteState Blueprint");
	AssetColor = FColor(0, 96, 128);
	AssetClass = UFiniteStateBlueprint::StaticClass();
	FactoryClass = UFiniteStateBlueprintFactory::StaticClass();
}

TSharedRef<FBlueprintEditorBase> FFiniteStateBlueprintActions::CreateBlueprintEditor() const
{
	return MakeShareable(new FFiniteStateBlueprintEditor());
}

#undef LOCTEXT_NAMESPACE
