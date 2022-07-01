// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_FiniteStateBlueprint.h"
#include "Misc/MessageDialog.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "FSM/FiniteStateBlueprintFactory.h"
#include "FSM/FinteStateEditor.h"
#include "FSM/Base/FiniteStateBase.h"
#include "FSM/Base/FiniteStateBlueprint.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FAssetTypeActions_FiniteStateBlueprint::FAssetTypeActions_FiniteStateBlueprint(EAssetTypeCategories::Type InAssetCategory) { AssetCategory = InAssetCategory; }

FText FAssetTypeActions_FiniteStateBlueprint::GetName() const { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_FiniteStateBlueprint", "FiniteState Blueprint"); }

FColor FAssetTypeActions_FiniteStateBlueprint::GetTypeColor() const { return FColor(0, 96, 128); }

void FAssetTypeActions_FiniteStateBlueprint::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for(auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Blueprint = Cast<UBlueprint>(*ObjIt);
		if(Blueprint)
		{
			bool bLetOpen = true;
			if(!Blueprint->ParentClass)
			{
				bLetOpen = EAppReturnType::Yes == FMessageDialog::Open(
					EAppMsgType::YesNo,
					LOCTEXT("FailedToLoadFiniteStateBlueprintWithContinue", "FiniteState Blueprint could not be loaded because it derives from an invalid class.  Check to make sure the parent class for this blueprint hasn't been removed! Do you want to continue (it can crash the editor)?")
				);
			}

			if(bLetOpen)
			{
				TSharedRef<FFiniteStateEditor> NewEditor(new FFiniteStateEditor());

				TArray<UBlueprint*> Blueprints;
				Blueprints.Add(Blueprint);

				NewEditor->InitFiniteStateEditor(Mode, EditWithinLevelEditor, Blueprints, ShouldUseDataOnlyEditor(Blueprint));
			}
		}
		else { FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FailedToLoadFiniteStateBlueprint", "FiniteState Blueprint could not be loaded because it derives from an invalid class.  Check to make sure the parent class for this blueprint hasn't been removed!")); }
	}
}

bool FAssetTypeActions_FiniteStateBlueprint::ShouldUseDataOnlyEditor(const UBlueprint* Blueprint) const
{
	return FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsLevelScriptBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsInterfaceBlueprint(Blueprint)
		&& !Blueprint->bForceFullEditor
		&& !Blueprint->bIsNewlyCreated;
}

UClass* FAssetTypeActions_FiniteStateBlueprint::GetSupportedClass() const { return UFiniteStateBlueprint::StaticClass(); }

UFactory* FAssetTypeActions_FiniteStateBlueprint::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UFiniteStateBlueprintFactory* FiniteStateBlueprintFactory = NewObject<UFiniteStateBlueprintFactory>();
	FiniteStateBlueprintFactory->ParentClass = TSubclassOf<UFiniteStateBase>(*InBlueprint->GeneratedClass);
	return FiniteStateBlueprintFactory;
}

#undef LOCTEXT_NAMESPACE
