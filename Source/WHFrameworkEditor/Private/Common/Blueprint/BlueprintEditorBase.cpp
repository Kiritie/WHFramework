// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/Blueprint/BlueprintEditorBase.h"

#include "Editor.h"
#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "FBlueprintEditorBase"

//////////////////////////////////////////////////////////////////////////
/// BlueprintEditorBase
FBlueprintEditorBase::FBlueprintEditorBase()
{
	BlueprintClass = nullptr;
	GraphSchemaClass = nullptr;
	
	ToolkitFName = FName("BlueprintEditor");
	BaseToolkitName = LOCTEXT("BlueprintEditorAppLabel", "Blueprint Editor");
	ToolkitNameFormat = LOCTEXT("BlueprintEditorToolkitName", "{ObjectName}{DirtyState}");

	WorldCentricTabPrefix = TEXT("BlueprintEditor");
	WorldCentricTabColorScale = FLinearColor::White;
}

FBlueprintEditorBase::~FBlueprintEditorBase()
{
	// NOTE: Any tabs that we still have hanging out when destroyed will be cleaned up by FBaseToolkit's destructor
}

void FBlueprintEditorBase::InitBlueprintEditorBase(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode)
{
	InitBlueprintEditor(Mode, InitToolkitHost, InBlueprints, bShouldOpenInDefaultsMode);

	for(auto Blueprint : InBlueprints)
	{
		EnsureBlueprintIsUpToDate(Blueprint);
	}
}

void FBlueprintEditorBase::EnsureBlueprintIsUpToDate(UBlueprint* Blueprint)
{
	#if WITH_EDITORONLY_DATA
	for(UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		// remove the default event graph, if it exists, from existing Gameplay Ability Blueprints
		if(Graph->GetName() == "EventGraph" && Graph->Nodes.Num() == 0)
		{
			check(!Graph->Schema->GetClass()->IsChildOf(GraphSchemaClass));
			FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph);
			break;
		}
	}
	#endif
}

FName FBlueprintEditorBase::GetToolkitFName() const { return ToolkitFName; }

FText FBlueprintEditorBase::GetBaseToolkitName() const { return BaseToolkitName; }

FText FBlueprintEditorBase::GetToolkitName() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();

	check(EditingObjs.Num() > 0);

	FFormatNamedArguments Args;

	const UObject* EditingObject = EditingObjs[0];

	const bool bDirtyState = EditingObject->GetOutermost()->IsDirty();

	Args.Add(TEXT("ObjectName"), FText::FromString(EditingObject->GetName()));
	Args.Add(TEXT("DirtyState"), bDirtyState ? FText::FromString(TEXT("*")) : FText::GetEmpty());
	return FText::Format(ToolkitNameFormat, Args);
}

FText FBlueprintEditorBase::GetToolkitToolTipText() const
{
	const UObject* EditingObject = GetEditingObject();

	check(EditingObject != NULL);

	return GetToolTipTextForObject(EditingObject);
}

FString FBlueprintEditorBase::GetWorldCentricTabPrefix() const { return WorldCentricTabPrefix; }

FLinearColor FBlueprintEditorBase::GetWorldCentricTabColorScale() const { return WorldCentricTabColorScale; }

FString FBlueprintEditorBase::GetDocumentationLink() const
{
	return FBlueprintEditor::GetDocumentationLink(); // todo: point this at the correct documentation
}

UBlueprint* FBlueprintEditorBase::GetBlueprintObj() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();
	for(int32 i = 0; i < EditingObjs.Num(); ++i) { if(EditingObjs[i]->IsA(BlueprintClass)) { return (UBlueprint*)EditingObjs[i]; } }
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
