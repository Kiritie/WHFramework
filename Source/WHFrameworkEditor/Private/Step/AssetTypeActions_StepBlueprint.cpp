// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_StepBlueprint.h"
#include "Misc/MessageDialog.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Step/StepBlueprintFactory.h"
#include "Step/StepEditor.h"
#include "Step/Base/StepBlueprint.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FAssetTypeActions_StepBlueprint::FAssetTypeActions_StepBlueprint(EAssetTypeCategories::Type InAssetCategory) { AssetCategory = InAssetCategory; }

FText FAssetTypeActions_StepBlueprint::GetName() const { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_StepBlueprint", "Step Blueprint"); }

FColor FAssetTypeActions_StepBlueprint::GetTypeColor() const { return FColor(0, 96, 128); }

void FAssetTypeActions_StepBlueprint::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
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
					LOCTEXT("FailedToLoadStepBlueprintWithContinue", "Step Blueprint could not be loaded because it derives from an invalid class.  Check to make sure the parent class for this blueprint hasn't been removed! Do you want to continue (it can crash the editor)?")
				);
			}

			if(bLetOpen)
			{
				TSharedRef<FStepEditor> NewEditor(new FStepEditor());

				TArray<UBlueprint*> Blueprints;
				Blueprints.Add(Blueprint);

				NewEditor->InitStepEditor(Mode, EditWithinLevelEditor, Blueprints, ShouldUseDataOnlyEditor(Blueprint));
			}
		}
		else { FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FailedToLoadStepBlueprint", "Step Blueprint could not be loaded because it derives from an invalid class.  Check to make sure the parent class for this blueprint hasn't been removed!")); }
	}
}

bool FAssetTypeActions_StepBlueprint::ShouldUseDataOnlyEditor(const UBlueprint* Blueprint) const
{
	return FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsLevelScriptBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsInterfaceBlueprint(Blueprint)
		&& !Blueprint->bForceFullEditor
		&& !Blueprint->bIsNewlyCreated;
}

UClass* FAssetTypeActions_StepBlueprint::GetSupportedClass() const { return UStepBlueprint::StaticClass(); }

UFactory* FAssetTypeActions_StepBlueprint::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UStepBlueprintFactory* StepBlueprintFactory = NewObject<UStepBlueprintFactory>();
	StepBlueprintFactory->ParentClass = TSubclassOf<UStepBase>(*InBlueprint->GeneratedClass);
	return StepBlueprintFactory;
}

#undef LOCTEXT_NAMESPACE
