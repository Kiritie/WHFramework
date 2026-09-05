#include "Dialogue/Asset/DialogueAssetActions.h"
#include "CoreMinimal.h"
#include "Dialogue/DialogueEditor.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"


FDialogueAssetActions::FDialogueAssetActions(uint32 InAssetCategory)
	: MyAssetCategory(InAssetCategory)
{
}

FText FDialogueAssetActions::GetName() const
{
	return LOCTEXT("FDialogueAssetActionsName", "Dialogue");
}

FColor FDialogueAssetActions::GetTypeColor() const
{
	return FColor(255, 55, 220);
}

UClass* FDialogueAssetActions::GetSupportedClass() const
{
	return UDialogueAsset::StaticClass();
}

void FDialogueAssetActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UDialogueAsset* Dialogue = Cast<UDialogueAsset>(*ObjIt))
		{
			FDialogueEditorModule::Get().CreateDialogueEditor(Mode, EditWithinLevelEditor, Dialogue);
		}
	}
}

uint32 FDialogueAssetActions::GetCategories()
{
	return MyAssetCategory; //affects filters
}

#undef LOCTEXT_NAMESPACE
