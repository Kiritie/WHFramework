// Copyright Epic Games, Inc. All Rights Reserved.

#include "Step/StepBlueprintActions.h"

#include "Misc/MessageDialog.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Step/StepBlueprintFactory.h"
#include "Step/StepEditor.h"
#include "Step/Base/StepBase.h"
#include "Step/Base/StepBlueprint.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FStepBlueprintActions::FStepBlueprintActions(EAssetTypeCategories::Type InAssetCategory) { AssetCategory = InAssetCategory; }

FText FStepBlueprintActions::GetName() const { return NSLOCTEXT("AssetTypeActions", "StepBlueprintActions", "Step Blueprint"); }

FColor FStepBlueprintActions::GetTypeColor() const { return FColor(0, 96, 128); }

void FStepBlueprintActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
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

bool FStepBlueprintActions::ShouldUseDataOnlyEditor(const UBlueprint* Blueprint) const
{
	return FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsLevelScriptBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsInterfaceBlueprint(Blueprint)
		&& !Blueprint->bForceFullEditor
		&& !Blueprint->bIsNewlyCreated;
}

UClass* FStepBlueprintActions::GetSupportedClass() const { return UStepBlueprint::StaticClass(); }

UFactory* FStepBlueprintActions::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UStepBlueprintFactory* StepBlueprintFactory = NewObject<UStepBlueprintFactory>();
	StepBlueprintFactory->ParentClass = TSubclassOf<UStepBase>(*InBlueprint->GeneratedClass);
	return StepBlueprintFactory;
}

#undef LOCTEXT_NAMESPACE
