// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/Blueprint/BlueprintActionsBase.h"

#include "Common/Blueprint/BlueprintEditorBase.h"
#include "Common/Blueprint/BlueprintFactoryBase.h"
#include "Misc/MessageDialog.h"
#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FBlueprintActionsBase::FBlueprintActionsBase(EAssetTypeCategories::Type InAssetCategory)
{
	AssetCategory = InAssetCategory;

	AssetName = NSLOCTEXT("AssetTypeActions", "BlueprintActionsBase", "Blueprint Base");
	AssetColor = FColor(0, 96, 128);
	AssetClass = UBlueprint::StaticClass();
	FactoryClass = UBlueprintFactoryBase::StaticClass();
}

void FBlueprintActionsBase::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
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
					LOCTEXT("FailedToLoadBlueprintWithContinue", "Blueprint could not be loaded because it derives from an invalid class.  Check to make sure the parent class for this blueprint hasn't been removed! Do you want to continue (it can crash the editor)?")
				);
			}

			if(bLetOpen)
			{
				const TSharedRef<FBlueprintEditorBase> NewEditor = CreateBlueprintEditor();

				TArray<UBlueprint*> Blueprints;
				Blueprints.Add(Blueprint);

				NewEditor->InitBlueprintEditorBase(Mode, EditWithinLevelEditor, Blueprints, ShouldUseDataOnlyEditor(Blueprint));
			}
		}
		else { FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FailedToLoadBlueprint", "Blueprint could not be loaded because it derives from an invalid class.  Check to make sure the parent class for this blueprint hasn't been removed!")); }
	}
}

TSharedRef<FBlueprintEditorBase> FBlueprintActionsBase::CreateBlueprintEditor() const
{
	return MakeShareable(new FBlueprintEditorBase());
}

UFactory* FBlueprintActionsBase::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UBlueprintFactoryBase* BlueprintFactory = NewObject<UBlueprintFactoryBase>(FactoryClass);
	BlueprintFactory->ParentClass = *InBlueprint->GeneratedClass;
	return BlueprintFactory;
}

bool FBlueprintActionsBase::ShouldUseDataOnlyEditor(const UBlueprint* Blueprint) const
{
	return FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsLevelScriptBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsInterfaceBlueprint(Blueprint)
		&& !Blueprint->bForceFullEditor
		&& !Blueprint->bIsNewlyCreated;
}

#undef LOCTEXT_NAMESPACE
