#include "Step/StepEditorTypes.h"

#include "Step/Base/StepAsset.h"
#include "Step/Base/StepBase.h"

FStepClassFilter::FStepClassFilter()
{
	DisallowedClassFlags = CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_HideDropDown;
	AllowedChildrenOfClasses.Add(UStepBase::StaticClass());
	bIncludeBaseClasses = true;
	bIncludeParentClasses = true;
}

bool FStepClassFilter::IsClassAllowed(UClass* InClass)
{
	const TSharedPtr<FStepEditor> Editor = StepEditor.Pin();
	return Editor && Editor->GetEditingAsset<UStepAsset>()->CanAddStep(InClass);
}

FStepEditorCommands::FStepEditorCommands()
	: TCommands<FStepEditorCommands>(FName("StepEditor"), NSLOCTEXT("StepEditor", "Commands", "Step Editor Commands"), NAME_None, FWHFrameworkEditorStyle::GetStyleSetName())
{
}

#define LOCTEXT_NAMESPACE "StepEditorTypes"
void FStepEditorCommands::RegisterCommands()
{
	UI_COMMAND(OpenStepEditorWindow, "Step Editor", "Open the step graph editor", EUserInterfaceActionType::Button, FInputChord());
}
#undef LOCTEXT_NAMESPACE
