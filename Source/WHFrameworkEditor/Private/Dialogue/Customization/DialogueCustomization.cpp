#include "Dialogue/Customization/DialogueCustomization.h"
#include "CoreMinimal.h"
#include "Dialogue/DialogueEditorTypes.h"
#include "Dialogue/Base/DialogueAsset.h"
#include "Dialogue/Slate/SDialogueViewportWidget.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Editor/PropertyEditor/Public/DetailLayoutBuilder.h"
#include "Editor/PropertyEditor/Public/DetailCategoryBuilder.h"
#include "Editor/UnrealEd/Public/ScopedTransaction.h"


#define LOCTEXT_NAMESPACE "DialoguePluginSettingsDetails"

TSharedRef<IDetailCustomization> FDialogueCustomization::MakeInstance(TSharedRef<FDialogueEditorState> InState)
{
	TSharedRef<FDialogueCustomization> Instance = MakeShared<FDialogueCustomization>();
	Instance->EditingState = InState;
	return Instance;
}

void FDialogueCustomization::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	DetailLayoutBuilder = &DetailLayout;

	const TSharedPtr<IPropertyHandle> DataProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDialogueAsset, Data));
	DetailLayout.HideProperty(DataProperty);
	const TSharedPtr<IPropertyHandle> NextNodeProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDialogueAsset, NextNodeId));
	DetailLayout.HideProperty(NextNodeProperty);

	// Create a category so this is displayed early in the properties
	IDetailCategoryBuilder& MyCategory = DetailLayout.EditCategory("Dialogue Editor");
	IDetailCategoryBuilder& CurrentNodeCategory = DetailLayout.EditCategory("Current Node", LOCTEXT("CurrentNode", "Current Node"), ECategoryPriority::Important);

	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;

	DetailLayout.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	TArray<UObject*> StrongObjects;
	CopyFromWeakArray(StrongObjects, ObjectsBeingCustomized);

	if (StrongObjects.Num() == 0) return;

	UDialogueAsset* Dialogue = Cast<UDialogueAsset>(StrongObjects[0]);

	if (Dialogue && EditingState && EditingState->CurrentNodeId != -1 && EditingState->CurrentNodeId != 0) //display current node details:
	{
		int32 index;
		FDialogueNode CurrentNode = Dialogue->GetNodeById(EditingState->CurrentNodeId, index);
		if (!Dialogue->Data.IsValidIndex(index)) return;

		CurrentNodeCategory.AddCustomRow(LOCTEXT("Text", "Text"))
			.WholeRowContent()
			[
				SNew(STextBlock).Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("Text", "Text"))
			];

		CurrentNodeCategory.AddCustomRow(LOCTEXT("TextValue", "TextValue"))
			.WholeRowContent()
			[
				SNew(SBox)
				.HeightOverride(100)
				[
					SNew(SMultiLineEditableTextBox).Text(CurrentNode.Text)
					.AutoWrapText(true)
					.OnTextCommitted(this, &FDialogueCustomization::TextCommited, Dialogue, EditingState->CurrentNodeId)
					.ModiferKeyForNewLine(EModifierKey::Shift)
				]

			];

		const TSharedPtr<IPropertyHandleArray> Array = DataProperty->AsArray();
		const TSharedPtr<IPropertyHandle> Child = Array->GetElement(index);
		const TSharedPtr<IPropertyHandle> IsPlayerField = Child->GetChildHandle("isPlayer");
		const TSharedPtr<IPropertyHandle> EventsField = Child->GetChildHandle("Events");
		const TSharedPtr<IPropertyHandle> ConditionsField = Child->GetChildHandle("Conditions");
		const TSharedPtr<IPropertyHandle> SoundField = Child->GetChildHandle("Sound");
		const TSharedPtr<IPropertyHandle> DialogueWaveField = Child->GetChildHandle("DialogueWave");

		CurrentNodeCategory.AddProperty(IsPlayerField);
		CurrentNodeCategory.AddProperty(EventsField);

		/*
		* Customizing Conditions Row
		*/
		IDetailPropertyRow * ConditionDetailsRow = &CurrentNodeCategory.AddProperty(ConditionsField);

		//TSharedPtr<SWidget> DefaultNameWidget;
		//TSharedPtr<SWidget> DefaultValueWidget;
		//FDetailWidgetRow DefaultWidgetRow;
		//ConditionDetailsRow->GetDefaultWidgets(DefaultNameWidget, DefaultValueWidget, DefaultWidgetRow);
		//
		//FDetailWidgetRow & CustomRow = ConditionDetailsRow->CustomWidget(false); // erases the default contents of the row
		//CustomRow.NameContent()
		//[
		//	DefaultNameWidget.ToSharedRef()
		//]
		//.ValueContent()
		//.MinDesiredWidth(170.0f)
		//[
		//	DefaultValueWidget.ToSharedRef()
		//];

		//uint32 children = 0;
		//ConditionsField->GetNumChildren(children);
		//for (uint32 i = 0; i < children; i++)
		//{
		//	TSharedPtr<IPropertyHandle> childHandle = ConditionsField->GetChildHandle(i);
		//	IDetailPropertyRow * subConditionRow = &CurrentNodeCategory.AddProperty(childHandle);
		//	subConditionRow->ShouldAutoExpand(true);
		//}
		/*
		* end of conditions customization
		*/

		CurrentNodeCategory.AddProperty(SoundField);
		CurrentNodeCategory.AddProperty(DialogueWaveField);
	}

}

void FDialogueCustomization::TextCommited(const FText& NewText, ETextCommit::Type CommitInfo, UDialogueAsset* Dialogue, int32 id)
{
	int32 index;
	FDialogueNode CurrentNode = Dialogue->GetNodeById(id, index);
	if (!Dialogue->Data.IsValidIndex(index)) return;

	// we don't commit text if it hasn't changed
	if (Dialogue->Data[index].Text.EqualTo(NewText))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("TextCommited", "Edited Node Text"));
	Dialogue->Modify();

	Dialogue->Data[index].Text = NewText;
}

#undef LOCTEXT_NAMESPACE
