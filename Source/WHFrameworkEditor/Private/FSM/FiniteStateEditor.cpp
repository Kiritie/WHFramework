// Copyright Epic Games, Inc. All Rights Reserved.

#include "FSM/FiniteStateEditor.h"


#if WITH_EDITOR
#include "Editor.h"
#endif

#include "Kismet2/BlueprintEditorUtils.h"
#include "FSM/FiniteStateGraphSchema.h"
#include "FSM/Base/FiniteStateBlueprint.h"

#define LOCTEXT_NAMESPACE "FFiniteStateEditor"


/////////////////////////////////////////////////////
// FFiniteStateEditor

FFiniteStateEditor::FFiniteStateEditor()
{
	// todo: Do we need to register a callback for when properties are changed?
}

FFiniteStateEditor::~FFiniteStateEditor()
{
	// NOTE: Any tabs that we still have hanging out when destroyed will be cleaned up by FBaseToolkit's destructor
}

void FFiniteStateEditor::InitFiniteStateEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode)
{
	InitBlueprintEditor(Mode, InitToolkitHost, InBlueprints, bShouldOpenInDefaultsMode);

	for(auto Blueprint : InBlueprints) { EnsureFiniteStateBlueprintIsUpToDate(Blueprint); }
}

void FFiniteStateEditor::EnsureFiniteStateBlueprintIsUpToDate(UBlueprint* Blueprint)
{
	#if WITH_EDITORONLY_DATA
	for(UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		// remove the default event graph, if it exists, from existing Gameplay Ability Blueprints
		if(Graph->GetName() == "EventGraph" && Graph->Nodes.Num() == 0)
		{
			check(!Graph->Schema->GetClass()->IsChildOf<UFiniteStateGraphSchema>());
			FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph);
			break;
		}
	}
	#endif
}

// FRED_TODO: don't merge this back
// FName FFiniteStateEditor::GetToolkitContextFName() const
// {
// 	return GetToolkitFName();
// }

FName FFiniteStateEditor::GetToolkitFName() const { return FName("FiniteStateEditor"); }

FText FFiniteStateEditor::GetBaseToolkitName() const { return LOCTEXT("FiniteStateEditorAppLabel", "FiniteState Editor"); }

FText FFiniteStateEditor::GetToolkitName() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();

	check(EditingObjs.Num() > 0);

	FFormatNamedArguments Args;

	const UObject* EditingObject = EditingObjs[0];

	const bool bDirtyState = EditingObject->GetOutermost()->IsDirty();

	Args.Add(TEXT("ObjectName"), FText::FromString(EditingObject->GetName()));
	Args.Add(TEXT("DirtyState"), bDirtyState ? FText::FromString(TEXT("*")) : FText::GetEmpty());
	return FText::Format(LOCTEXT("FiniteStateToolkitName", "{ObjectName}{DirtyState}"), Args);
}

FText FFiniteStateEditor::GetToolkitToolTipText() const
{
	const UObject* EditingObject = GetEditingObject();

	check(EditingObject != NULL);

	return FAssetEditorToolkit::GetToolTipTextForObject(EditingObject);
}

FString FFiniteStateEditor::GetWorldCentricTabPrefix() const { return TEXT("FiniteStateEditor"); }

FLinearColor FFiniteStateEditor::GetWorldCentricTabColorScale() const { return FLinearColor::White; }

UBlueprint* FFiniteStateEditor::GetBlueprintObj() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();
	for(int32 i = 0; i < EditingObjs.Num(); ++i) { if(EditingObjs[i]->IsA<UFiniteStateBlueprint>()) { return (UBlueprint*)EditingObjs[i]; } }
	return nullptr;
}

FString FFiniteStateEditor::GetDocumentationLink() const
{
	return FBlueprintEditor::GetDocumentationLink(); // todo: point this at the correct documentation
}

#undef LOCTEXT_NAMESPACE
