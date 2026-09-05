#include "Dialogue/Asset/DialogueAssetFactory.h"
#include "Dialogue/DialogueEditor.h"
#include "Dialogue/Base/DialogueAsset.h"

#define LOCTEXT_NAMESPACE "DialoguePluginEditor"

UDialogueAssetFactory::UDialogueAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UDialogueAsset::StaticClass();
}

UObject* UDialogueAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UDialogueAsset* NewObjectAsset = NewObject<UDialogueAsset>(InParent, Class, Name, Flags | RF_Transactional);
	FDialogueNode Start;
	Start.id = 0;
	NewObjectAsset->Data.Add(Start);
	return NewObjectAsset;
}

uint32 UDialogueAssetFactory::GetMenuCategories() const
{
	return FDialogueEditorModule::GameAssetCategory;
}

FText UDialogueAssetFactory::GetDisplayName() const
{
	return LOCTEXT("DialogueText", "Dialogue");
}

FString UDialogueAssetFactory::GetDefaultNewAssetName() const
{
	return FString(TEXT("NewDialogue"));
}

#undef LOCTEXT_NAMESPACE