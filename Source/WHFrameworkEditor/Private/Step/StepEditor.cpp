// Copyright Epic Games, Inc. All Rights Reserved.

#include "Step/StepEditor.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#include "Kismet2/BlueprintEditorUtils.h"
#include "Step/StepGraphSchema.h"
#include "Step/Base/StepBlueprint.h"

#define LOCTEXT_NAMESPACE "FStepEditor"


/////////////////////////////////////////////////////
// FStepEditor

FStepBlueprintEditor::FStepBlueprintEditor()
{
	// todo: Do we need to register a callback for when properties are changed?
}

FStepBlueprintEditor::~FStepBlueprintEditor()
{
	// NOTE: Any tabs that we still have hanging out when destroyed will be cleaned up by FBaseToolkit's destructor
}

void FStepBlueprintEditor::InitStepEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode)
{
	InitBlueprintEditor(Mode, InitToolkitHost, InBlueprints, bShouldOpenInDefaultsMode);

	for(auto Blueprint : InBlueprints) { EnsureStepBlueprintIsUpToDate(Blueprint); }
}

void FStepBlueprintEditor::EnsureStepBlueprintIsUpToDate(UBlueprint* Blueprint)
{
	#if WITH_EDITORONLY_DATA
	for(UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		// remove the default event graph, if it exists, from existing Gameplay Ability Blueprints
		if(Graph->GetName() == "EventGraph" && Graph->Nodes.Num() == 0)
		{
			check(!Graph->Schema->GetClass()->IsChildOf<UStepGraphSchema>());
			FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph);
			break;
		}
	}
	#endif
}

// FRED_TODO: don't merge this back
// FName FStepEditor::GetToolkitContextFName() const
// {
// 	return GetToolkitFName();
// }

FName FStepBlueprintEditor::GetToolkitFName() const { return FName("StepEditor"); }

FText FStepBlueprintEditor::GetBaseToolkitName() const { return LOCTEXT("StepEditorAppLabel", "Step Editor"); }

FText FStepBlueprintEditor::GetToolkitName() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();

	check(EditingObjs.Num() > 0);

	FFormatNamedArguments Args;

	const UObject* EditingObject = EditingObjs[0];

	const bool bDirtyState = EditingObject->GetOutermost()->IsDirty();

	Args.Add(TEXT("ObjectName"), FText::FromString(EditingObject->GetName()));
	Args.Add(TEXT("DirtyState"), bDirtyState ? FText::FromString(TEXT("*")) : FText::GetEmpty());
	return FText::Format(LOCTEXT("StepToolkitName", "{ObjectName}{DirtyState}"), Args);
}

FText FStepBlueprintEditor::GetToolkitToolTipText() const
{
	const UObject* EditingObject = GetEditingObject();

	check(EditingObject != NULL);

	return FAssetEditorToolkit::GetToolTipTextForObject(EditingObject);
}

FString FStepBlueprintEditor::GetWorldCentricTabPrefix() const { return TEXT("StepEditor"); }

FLinearColor FStepBlueprintEditor::GetWorldCentricTabColorScale() const { return FLinearColor::White; }

UBlueprint* FStepBlueprintEditor::GetBlueprintObj() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();
	for(int32 i = 0; i < EditingObjs.Num(); ++i) { if(EditingObjs[i]->IsA<UStepBlueprint>()) { return (UBlueprint*)EditingObjs[i]; } }
	return nullptr;
}

FString FStepBlueprintEditor::GetDocumentationLink() const
{
	return FBlueprintEditor::GetDocumentationLink(); // todo: point this at the correct documentation
}

#undef LOCTEXT_NAMESPACE
