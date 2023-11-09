// Copyright Epic Games, Inc. All Rights Reserved.

#include "Main/ModuleBlueprintActions.h"

#include "Misc/MessageDialog.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Main/ModuleBlueprintFactory.h"
#include "Main/ModuleEditor.h"
#include "Main/Base/ModuleBase.h"
#include "Main/Base/ModuleBlueprint.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FModuleBlueprintActions::FModuleBlueprintActions(EAssetTypeCategories::Type InAssetCategory) { AssetCategory = InAssetCategory; }

FText FModuleBlueprintActions::GetName() const { return NSLOCTEXT("AssetTypeActions", "ModuleBlueprintActions", "Module Blueprint"); }

FColor FModuleBlueprintActions::GetTypeColor() const { return FColor(0, 96, 128); }

void FModuleBlueprintActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
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
					LOCTEXT("FailedToLoadModuleBlueprintWithContinue", "Module Blueprint could not be loaded because it derives from an invalid class.  Check to make sure the parent class for this blueprint hasn't been removed! Do you want to continue (it can crash the editor)?")
				);
			}

			if(bLetOpen)
			{
				TSharedRef<FModuleBlueprintEditor> NewEditor(new FModuleBlueprintEditor());

				TArray<UBlueprint*> Blueprints;
				Blueprints.Add(Blueprint);

				NewEditor->InitModuleEditor(Mode, EditWithinLevelEditor, Blueprints, ShouldUseDataOnlyEditor(Blueprint));
			}
		}
		else { FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FailedToLoadModuleBlueprint", "Module Blueprint could not be loaded because it derives from an invalid class.  Check to make sure the parent class for this blueprint hasn't been removed!")); }
	}
}

bool FModuleBlueprintActions::ShouldUseDataOnlyEditor(const UBlueprint* Blueprint) const
{
	return FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsLevelScriptBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsInterfaceBlueprint(Blueprint)
		&& !Blueprint->bForceFullEditor
		&& !Blueprint->bIsNewlyCreated;
}

UClass* FModuleBlueprintActions::GetSupportedClass() const { return UModuleBlueprint::StaticClass(); }

UFactory* FModuleBlueprintActions::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UModuleBlueprintFactory* ModuleBlueprintFactory = NewObject<UModuleBlueprintFactory>();
	ModuleBlueprintFactory->ParentClass = TSubclassOf<UModuleBase>(*InBlueprint->GeneratedClass);
	return ModuleBlueprintFactory;
}

#undef LOCTEXT_NAMESPACE
