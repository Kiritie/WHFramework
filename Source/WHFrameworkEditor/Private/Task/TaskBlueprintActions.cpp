// Copyright Epic Games, Inc. All Rights Reserved.

#include "Task/TaskBlueprintActions.h"

#include "Misc/MessageDialog.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Task/TaskBlueprintFactory.h"
#include "Task/TaskEditor.h"
#include "Task/Base/TaskBase.h"
#include "Task/Base/TaskBlueprint.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FTaskBlueprintActions::FTaskBlueprintActions(EAssetTypeCategories::Type InAssetCategory) { AssetCategory = InAssetCategory; }

FText FTaskBlueprintActions::GetName() const { return NSLOCTEXT("AssetTypeActions", "TaskBlueprintActions", "Task Blueprint"); }

FColor FTaskBlueprintActions::GetTypeColor() const { return FColor(0, 96, 128); }

void FTaskBlueprintActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
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
					LOCTEXT("FailedToLoadTaskBlueprintWithContinue", "Task Blueprint could not be loaded because it derives from an invalid class.  Check to make sure the parent class for this blueprint hasn't been removed! Do you want to continue (it can crash the editor)?")
				);
			}

			if(bLetOpen)
			{
				TSharedRef<FTaskBlueprintEditor> NewEditor(new FTaskBlueprintEditor());

				TArray<UBlueprint*> Blueprints;
				Blueprints.Add(Blueprint);

				NewEditor->InitTaskEditor(Mode, EditWithinLevelEditor, Blueprints, ShouldUseDataOnlyEditor(Blueprint));
			}
		}
		else { FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FailedToLoadTaskBlueprint", "Task Blueprint could not be loaded because it derives from an invalid class.  Check to make sure the parent class for this blueprint hasn't been removed!")); }
	}
}

bool FTaskBlueprintActions::ShouldUseDataOnlyEditor(const UBlueprint* Blueprint) const
{
	return FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsLevelScriptBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsInterfaceBlueprint(Blueprint)
		&& !Blueprint->bForceFullEditor
		&& !Blueprint->bIsNewlyCreated;
}

UClass* FTaskBlueprintActions::GetSupportedClass() const { return UTaskBlueprint::StaticClass(); }

UFactory* FTaskBlueprintActions::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UTaskBlueprintFactory* TaskBlueprintFactory = NewObject<UTaskBlueprintFactory>();
	TaskBlueprintFactory->ParentClass = TSubclassOf<UTaskBase>(*InBlueprint->GeneratedClass);
	return TaskBlueprintFactory;
}

#undef LOCTEXT_NAMESPACE
