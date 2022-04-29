// Copyright Epic Games, Inc. All Rights Reserved.

#include "Procedure/ProcedureEditor.h"

#include "EditorReimportHandler.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#include "Kismet2/BlueprintEditorUtils.h"
#include "Procedure/ProcedureGraphSchema.h"
#include "Procedure/Base/ProcedureBlueprint.h"

#define LOCTEXT_NAMESPACE "FProcedureEditor"


/////////////////////////////////////////////////////
// FProcedureEditor

FProcedureEditor::FProcedureEditor()
{
	// todo: Do we need to register a callback for when properties are changed?
}

FProcedureEditor::~FProcedureEditor()
{
	// NOTE: Any tabs that we still have hanging out when destroyed will be cleaned up by FBaseToolkit's destructor
}

void FProcedureEditor::InitProcedureEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode)
{
	InitBlueprintEditor(Mode, InitToolkitHost, InBlueprints, bShouldOpenInDefaultsMode);

	for(auto Blueprint : InBlueprints) { EnsureProcedureBlueprintIsUpToDate(Blueprint); }
}

void FProcedureEditor::EnsureProcedureBlueprintIsUpToDate(UBlueprint* Blueprint)
{
	#if WITH_EDITORONLY_DATA
	for(UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		// remove the default event graph, if it exists, from existing Gameplay Ability Blueprints
		if(Graph->GetName() == "EventGraph" && Graph->Nodes.Num() == 0)
		{
			check(!Graph->Schema->GetClass()->IsChildOf(UProcedureGraphSchema::StaticClass()));
			FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph);
			break;
		}
	}
	#endif
}

// FRED_TODO: don't merge this back
// FName FProcedureEditor::GetToolkitContextFName() const
// {
// 	return GetToolkitFName();
// }

FName FProcedureEditor::GetToolkitFName() const { return FName("ProcedureEditor"); }

FText FProcedureEditor::GetBaseToolkitName() const { return LOCTEXT("ProcedureEditorAppLabel", "Procedure Editor"); }

FText FProcedureEditor::GetToolkitName() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();

	check(EditingObjs.Num() > 0);

	FFormatNamedArguments Args;

	const UObject* EditingObject = EditingObjs[0];

	const bool bDirtyState = EditingObject->GetOutermost()->IsDirty();

	Args.Add(TEXT("ObjectName"), FText::FromString(EditingObject->GetName()));
	Args.Add(TEXT("DirtyState"), bDirtyState ? FText::FromString(TEXT("*")) : FText::GetEmpty());
	return FText::Format(LOCTEXT("ProcedureToolkitName", "{ObjectName}{DirtyState}"), Args);
}

FText FProcedureEditor::GetToolkitToolTipText() const
{
	const UObject* EditingObject = GetEditingObject();

	check(EditingObject != NULL);

	return FAssetEditorToolkit::GetToolTipTextForObject(EditingObject);
}

FString FProcedureEditor::GetWorldCentricTabPrefix() const { return TEXT("ProcedureEditor"); }

FLinearColor FProcedureEditor::GetWorldCentricTabColorScale() const { return FLinearColor::White; }

UBlueprint* FProcedureEditor::GetBlueprintObj() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();
	for(int32 i = 0; i < EditingObjs.Num(); ++i) { if(EditingObjs[i]->IsA<UProcedureBlueprint>()) { return (UBlueprint*)EditingObjs[i]; } }
	return nullptr;
}

FString FProcedureEditor::GetDocumentationLink() const
{
	return FBlueprintEditor::GetDocumentationLink(); // todo: point this at the correct documentation
}

#undef LOCTEXT_NAMESPACE
