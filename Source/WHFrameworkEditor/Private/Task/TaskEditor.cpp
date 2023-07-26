// Copyright Epic Games, Inc. All Rights Reserved.

#include "Task/TaskEditor.h"

#include "EditorReimportHandler.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#include "Kismet2/BlueprintEditorUtils.h"
#include "Task/TaskGraphSchema.h"
#include "Task/Base/TaskBlueprint.h"

#define LOCTEXT_NAMESPACE "FTaskEditor"


/////////////////////////////////////////////////////
// FTaskEditor

FTaskBlueprintEditor::FTaskBlueprintEditor()
{
	// todo: Do we need to register a callback for when properties are changed?
}

FTaskBlueprintEditor::~FTaskBlueprintEditor()
{
	// NOTE: Any tabs that we still have hanging out when destroyed will be cleaned up by FBaseToolkit's destructor
}

void FTaskBlueprintEditor::InitTaskEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode)
{
	InitBlueprintEditor(Mode, InitToolkitHost, InBlueprints, bShouldOpenInDefaultsMode);

	for(auto Blueprint : InBlueprints) { EnsureTaskBlueprintIsUpToDate(Blueprint); }
}

void FTaskBlueprintEditor::EnsureTaskBlueprintIsUpToDate(UBlueprint* Blueprint)
{
	#if WITH_EDITORONLY_DATA
	for(UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		// remove the default event graph, if it exists, from existing Gameplay Ability Blueprints
		if(Graph->GetName() == "EventGraph" && Graph->Nodes.Num() == 0)
		{
			check(!Graph->Schema->GetClass()->IsChildOf<UTaskGraphSchema>());
			FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph);
			break;
		}
	}
	#endif
}

// FRED_TODO: don't merge this back
// FName FTaskEditor::GetToolkitContextFName() const
// {
// 	return GetToolkitFName();
// }

FName FTaskBlueprintEditor::GetToolkitFName() const { return FName("TaskEditor"); }

FText FTaskBlueprintEditor::GetBaseToolkitName() const { return LOCTEXT("TaskEditorAppLabel", "Task Editor"); }

FText FTaskBlueprintEditor::GetToolkitName() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();

	check(EditingObjs.Num() > 0);

	FFormatNamedArguments Args;

	const UObject* EditingObject = EditingObjs[0];

	const bool bDirtyState = EditingObject->GetOutermost()->IsDirty();

	Args.Add(TEXT("ObjectName"), FText::FromString(EditingObject->GetName()));
	Args.Add(TEXT("DirtyState"), bDirtyState ? FText::FromString(TEXT("*")) : FText::GetEmpty());
	return FText::Format(LOCTEXT("TaskToolkitName", "{ObjectName}{DirtyState}"), Args);
}

FText FTaskBlueprintEditor::GetToolkitToolTipText() const
{
	const UObject* EditingObject = GetEditingObject();

	check(EditingObject != NULL);

	return FAssetEditorToolkit::GetToolTipTextForObject(EditingObject);
}

FString FTaskBlueprintEditor::GetWorldCentricTabPrefix() const { return TEXT("TaskEditor"); }

FLinearColor FTaskBlueprintEditor::GetWorldCentricTabColorScale() const { return FLinearColor::White; }

UBlueprint* FTaskBlueprintEditor::GetBlueprintObj() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();
	for(int32 i = 0; i < EditingObjs.Num(); ++i) { if(EditingObjs[i]->IsA<UTaskBlueprint>()) { return (UBlueprint*)EditingObjs[i]; } }
	return nullptr;
}

FString FTaskBlueprintEditor::GetDocumentationLink() const
{
	return FBlueprintEditor::GetDocumentationLink(); // todo: point this at the correct documentation
}

#undef LOCTEXT_NAMESPACE
