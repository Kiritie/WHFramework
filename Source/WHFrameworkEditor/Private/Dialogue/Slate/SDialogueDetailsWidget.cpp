#include "Dialogue/Slate/SDialogueDetailsWidget.h"

#include "Dialogue/DialogueEditor.h"
#include "Dialogue/Customization/DialogueCustomization.h"
#include "IDetailsView.h"

void SDialogueDetailsWidget::Construct(const FArguments& InArgs, TSharedPtr<FDialogueEditor> InEditor)
{
	Editor = InEditor;
	SSingleObjectDetailsPanel::Construct(SSingleObjectDetailsPanel::FArguments().HostCommandList(InEditor->GetToolkitCommands()), true, true);
	PropertyView->RegisterInstancedCustomPropertyLayout(UDialogueAsset::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FDialogueCustomization::MakeInstance, InEditor->GetEditingState()));
	PropertyView->SetObject(InEditor->GetDialogueBeingEdited(), true);
}
UObject* SDialogueDetailsWidget::GetObjectToObserve() const
{
	const TSharedPtr<FDialogueEditor> Pinned = Editor.Pin();
	return Pinned ? Pinned->GetDialogueBeingEdited() : nullptr;
}
void SDialogueDetailsWidget::Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime)
{
	SSingleObjectDetailsPanel::Tick(Geometry, CurrentTime, DeltaTime);
	const TSharedPtr<FDialogueEditor> Pinned = Editor.Pin();
	if (Pinned && Pinned->refreshDetails)
	{
		PropertyView->SetObject(GetObjectToObserve(), true);
		Pinned->refreshDetails = false;
	}
}
