// Copyright Epic Games, Inc. All Rights Reserved.

#include "Main/ModuleEditor.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#include "WHFrameworkEditorCommands.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Main/ModuleGraphSchema.h"
#include "Main/Base/ModuleBlueprint.h"
#include "Main/Widget/SModuleEditorWidget.h"

#define LOCTEXT_NAMESPACE "FModuleEditor"

static const FName ModuleEditorTabName("ModuleEditor");

IMPLEMENTATION_EDITOR_MODULE(FModuleEditor)

void FModuleEditor::StartupModule()
{
	// Register tabs
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ModuleEditorTabName, FOnSpawnTab::CreateRaw(this, &FModuleEditor::OnSpawnModuleEditorTab))
		.SetDisplayName(LOCTEXT("FModuleEditorTabTitle", "Module Editor"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FModuleEditor::ShutdownModule()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ModuleEditorTabName);
}

void FModuleEditor::RegisterCommands(const TSharedPtr<FUICommandList>& InCommands)
{
	InCommands->MapAction(
		FWHFrameworkEditorCommands::Get().OpenModuleEditorWindow,
		FExecuteAction::CreateRaw(this, &FModuleEditor::OnClickedModuleEditorButton),
		FCanExecuteAction());
}

void FModuleEditor::OnClickedModuleEditorButton()
{
	FGlobalTabmanager::Get()->TryInvokeTab(ModuleEditorTabName);
}

TSharedRef<SDockTab> FModuleEditor::OnSpawnModuleEditorTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab).TabRole(ETabRole::NomadTab)
	[
		SAssignNew(ModuleEditorWidget, SModuleEditorWidget)
	];
}

/////////////////////////////////////////////////////
// FModuleEditor

FModuleBlueprintEditor::FModuleBlueprintEditor()
{
	// todo: Do we need to register a callback for when properties are changed?
}

FModuleBlueprintEditor::~FModuleBlueprintEditor()
{
	// NOTE: Any tabs that we still have hanging out when destroyed will be cleaned up by FBaseToolkit's destructor
}

void FModuleBlueprintEditor::InitModuleEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode)
{
	InitBlueprintEditor(Mode, InitToolkitHost, InBlueprints, bShouldOpenInDefaultsMode);

	for(auto Blueprint : InBlueprints) { EnsureModuleBlueprintIsUpToDate(Blueprint); }
}

void FModuleBlueprintEditor::EnsureModuleBlueprintIsUpToDate(UBlueprint* Blueprint)
{
	#if WITH_EDITORONLY_DATA
	for(UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		// remove the default event graph, if it exists, from existing Gameplay Ability Blueprints
		if(Graph->GetName() == "EventGraph" && Graph->Nodes.Num() == 0)
		{
			check(!Graph->Schema->GetClass()->IsChildOf<UModuleGraphSchema>());
			FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph);
			break;
		}
	}
	#endif
}

// FRED_TODO: don't merge this back
// FName FModuleEditor::GetToolkitContextFName() const
// {
// 	return GetToolkitFName();
// }

FName FModuleBlueprintEditor::GetToolkitFName() const { return FName("ModuleEditor"); }

FText FModuleBlueprintEditor::GetBaseToolkitName() const { return LOCTEXT("ModuleEditorAppLabel", "Module Editor"); }

FText FModuleBlueprintEditor::GetToolkitName() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();

	check(EditingObjs.Num() > 0);

	FFormatNamedArguments Args;

	const UObject* EditingObject = EditingObjs[0];

	const bool bDirtyState = EditingObject->GetOutermost()->IsDirty();

	Args.Add(TEXT("ObjectName"), FText::FromString(EditingObject->GetName()));
	Args.Add(TEXT("DirtyState"), bDirtyState ? FText::FromString(TEXT("*")) : FText::GetEmpty());
	return FText::Format(LOCTEXT("ModuleToolkitName", "{ObjectName}{DirtyState}"), Args);
}

FText FModuleBlueprintEditor::GetToolkitToolTipText() const
{
	const UObject* EditingObject = GetEditingObject();

	check(EditingObject != NULL);

	return FAssetEditorToolkit::GetToolTipTextForObject(EditingObject);
}

FString FModuleBlueprintEditor::GetWorldCentricTabPrefix() const { return TEXT("ModuleEditor"); }

FLinearColor FModuleBlueprintEditor::GetWorldCentricTabColorScale() const { return FLinearColor::White; }

UBlueprint* FModuleBlueprintEditor::GetBlueprintObj() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();
	for(int32 i = 0; i < EditingObjs.Num(); ++i) { if(EditingObjs[i]->IsA<UModuleBlueprint>()) { return (UBlueprint*)EditingObjs[i]; } }
	return nullptr;
}

FString FModuleBlueprintEditor::GetDocumentationLink() const
{
	return FBlueprintEditor::GetDocumentationLink(); // todo: point this at the correct documentation
}

#undef LOCTEXT_NAMESPACE
