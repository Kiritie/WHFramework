// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_ProcedureBlueprint.h"
#include "Misc/MessageDialog.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Procedure/ProcedureBlueprintFactory.h"
#include "Procedure/ProcedureEditor.h"
#include "Procedure/Base/ProcedureBlueprint.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FAssetTypeActions_ProcedureBlueprint::FAssetTypeActions_ProcedureBlueprint(EAssetTypeCategories::Type InAssetCategory)
{
	AssetCategory = InAssetCategory;
}

FText FAssetTypeActions_ProcedureBlueprint::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ProcedureBlueprint", "Procedure Blueprint"); 
}

FColor FAssetTypeActions_ProcedureBlueprint::GetTypeColor() const
{
	return FColor(0, 96, 128);
}

void FAssetTypeActions_ProcedureBlueprint::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Blueprint = Cast<UBlueprint>(*ObjIt);
		if (Blueprint )
		{
			bool bLetOpen = true;
			if (!Blueprint->ParentClass)
			{
				bLetOpen = EAppReturnType::Yes == FMessageDialog::Open(
					EAppMsgType::YesNo, 
					LOCTEXT("FailedToLoadProcedureBlueprintWithContinue", "Procedure Blueprint could not be loaded because it derives from an invalid class.  Check to make sure the parent class for this blueprint hasn't been removed! Do you want to continue (it can crash the editor)?")
				);
			}
		
			if (bLetOpen)
			{
				TSharedRef< FProcedureEditor > NewEditor(new FProcedureEditor());

				TArray<UBlueprint*> Blueprints;
				Blueprints.Add(Blueprint);

				NewEditor->InitProcedureEditor(Mode, EditWithinLevelEditor, Blueprints, ShouldUseDataOnlyEditor(Blueprint));
			}
		}
		else
		{
			FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("FailedToLoadProcedureBlueprint", "Procedure Blueprint could not be loaded because it derives from an invalid class.  Check to make sure the parent class for this blueprint hasn't been removed!"));
		}
	}
}

bool FAssetTypeActions_ProcedureBlueprint::ShouldUseDataOnlyEditor(const UBlueprint* Blueprint) const
{
	return FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsLevelScriptBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsInterfaceBlueprint(Blueprint)
		&& !Blueprint->bForceFullEditor
		&& !Blueprint->bIsNewlyCreated;
}

UClass* FAssetTypeActions_ProcedureBlueprint::GetSupportedClass() const
{ 
	return UProcedureBlueprint::StaticClass(); 
}

UFactory* FAssetTypeActions_ProcedureBlueprint::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UProcedureBlueprintFactory* ProcedureBlueprintFactory = NewObject<UProcedureBlueprintFactory>();
	ProcedureBlueprintFactory->ParentClass = TSubclassOf<UProcedureBase>(*InBlueprint->GeneratedClass);
	return ProcedureBlueprintFactory;
}

#undef LOCTEXT_NAMESPACE
