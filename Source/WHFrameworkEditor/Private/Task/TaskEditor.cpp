// Copyright Epic Games, Inc. All Rights Reserved.

#include "Task/TaskEditor.h"
#include "Task/Slate/STaskGraphWidget.h"
#include "Task/Graph/TaskAssetGraphNode.h"
#include "EdGraphUtilities.h"

#include "Editor.h"
#include "WHFrameworkSlateStatics.h"
#include "Task/TaskModule.h"
#include "Task/Base/TaskAsset.h"
#include "Task/Base/TaskBlueprint.h"
#include "Task/Blueprint/TaskBlueprintActions.h"
#include "Task/Blueprint/TaskGraphSchema.h"
#include "Task/Customization/TaskCustomization.h"
#include "Task/TaskEditorTypes.h"
#include "Task/Slate/STaskDetailsWidget.h"
#include "IDetailsView.h"
#include "Task/Slate/STaskStatusWidget.h"

#define LOCTEXT_NAMESPACE "FTaskEditor"

//////////////////////////////////////////////////////////////////////////
/// TaskEditorModule
IMPLEMENTATION_EDITOR_MODULE(FTaskEditorModule)

FTaskEditorModule::FTaskEditorModule()
{
	AppIdentifier = FName("TaskEditorApp");
}

void FTaskEditorModule::StartupModule()
{
	FTaskEditorCommands::Register();
	GraphNodeFactory = CreateTaskGraphNodeFactory();
	FEdGraphUtilities::RegisterVisualNodeFactory(GraphNodeFactory);
}

void FTaskEditorModule::ShutdownModule()
{
	FEdGraphUtilities::UnregisterVisualNodeFactory(GraphNodeFactory);
	GraphNodeFactory.Reset();
	FTaskEditorCommands::Unregister();
}

void FTaskEditorModule::RegisterCommands(const TSharedPtr<FUICommandList>& Commands)
{
	Commands->MapAction(
		FTaskEditorCommands::Get().OpenTaskEditorWindow,
		FExecuteAction::CreateRaw(this, &FTaskEditorModule::OnClickedTaskEditorButton),
		FCanExecuteAction());
}

void FTaskEditorModule::RegisterMenus(const TSharedPtr<FUICommandList>& Commands)
{
	AddWindowMenu(FTaskEditorCommands::Get().OpenTaskEditorWindow, Commands);
	
	AddToolbarMenu(FTaskEditorCommands::Get().OpenTaskEditorWindow, Commands);
}

void FTaskEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, EAssetTypeCategories::Type AssetCategory, TArray<TSharedPtr<IAssetTypeActions>>& AssetTypeActions)
{
	const TSharedRef<IAssetTypeActions> AssetAction = MakeShareable(new FTaskBlueprintActions(AssetCategory));
	AssetTools.RegisterAssetTypeActions(AssetAction);
	AssetTypeActions.Add(AssetAction);
}

void FTaskEditorModule::RegisterCustomization(FPropertyEditorModule& PropertyEditor)
{
	FEditorModuleBase::RegisterCustomization(PropertyEditor);
	
	PropertyEditor.RegisterCustomClassLayout(FName("TaskBase"), FOnGetDetailCustomizationInstance::CreateStatic(&FTaskCustomization::MakeInstance));
}

void FTaskEditorModule::UnRegisterCustomization(FPropertyEditorModule& PropertyEditor)
{
	PropertyEditor.UnregisterCustomClassLayout(FName("TaskBase"));
}

TSharedRef<FTaskEditor> FTaskEditorModule::CreateTaskEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UTaskAsset* Task)
{
	TSharedRef< FTaskEditor > NewTaskEditor( new FTaskEditor() );
	NewTaskEditor->InitAssetEditorBase( Mode, InitToolkitHost, Task );
	return NewTaskEditor;
}

void FTaskEditorModule::OnClickedTaskEditorButton()
{
	if(const UTaskModule* TaskModule = UTaskModule::GetPtr(!UCommonModuleStatics::IsPlaying()))
	{
		for(auto Iter : !UCommonModuleStatics::IsPlaying() ? TaskModule->GetDefaultAssets() : TaskModule->GetAssets())
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Iter);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
/// TaskEditor
FTaskEditor::FTaskEditor()
{
	ToolkitFName = FName("TaskEditor");
	BaseToolkitName =  LOCTEXT("AppLabel", "Task Editor");
	MenuCategory = LOCTEXT("TaskEditor", "Task Editor");
	DefaultLayoutName = FName("TaskEditor_ContextGraphLayout_v2");
	WorldCentricTabPrefix =  LOCTEXT("WorldCentricTabPrefix", "Task ").ToString();
	WorldCentricTabColorScale =  FLinearColor( 0.0f, 0.0f, 0.2f, 0.5f );
}

FTaskEditor::~FTaskEditor()
{
	
}

void FTaskEditor::InitAssetEditorBase(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* Asset)
{
	Asset->SetFlags(RF_Transactional);
	GEditor->RegisterForUndo(this);
	FAssetEditorBase::InitAssetEditorBase(Mode, InitToolkitHost, Asset);
}

void FTaskEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorBase::RegisterTabSpawners(InTabManager);

	SAssignNewEd(DetailsWidget, STaskDetailsWidget, true)
		.TaskEditor(SharedThis(this));

	SAssignNewEd(StatusWidget, STaskStatusWidget, true)
		.TaskEditor(SharedThis(this));

	SAssignNew(GraphWidget, STaskGraphWidget).TaskEditor(SharedThis(this));
	GetToolkitCommands()->Append(GraphWidget->GetCommands().ToSharedRef());
	DetailsWidget->DetailsView->OnFinishedChangingProperties().AddSP(this, &FTaskEditor::OnTaskPropertyChanged);
	DetailsWidget->DetailsView->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateSP(GraphWidget.ToSharedRef(), &STaskGraphWidget::CanEdit));
	RefreshDetails();
	RegisterTrackedTabSpawner(InTabManager, "Graph", FOnSpawnTab::CreateSP(this, &FTaskEditor::SpawnGraphWidgetTab))
		.SetDisplayName(LOCTEXT("GraphTab", "Task Graph"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"));

	RegisterTrackedTabSpawner(InTabManager, "Details", FOnSpawnTab::CreateSP(this, &FTaskEditor::SpawnDetailsWidgetTab))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	RegisterTrackedTabSpawner(InTabManager, "Status", FOnSpawnTab::CreateSP(this, &FTaskEditor::SpawnStatusWidgetTab))
		.SetDisplayName(LOCTEXT("StatusTab", "Status"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.StatsViewer"));
}

void FTaskEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner("Graph");
	InTabManager->UnregisterTabSpawner("Details");
	InTabManager->UnregisterTabSpawner("Status");
}

TSharedRef<FTabManager::FLayout> FTaskEditor::CreateDefaultLayout()
{
	return FTabManager::NewLayout(DefaultLayoutName)->AddArea(
		FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
		->Split(FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)->SetSizeCoefficient(0.95f)
			->Split(FTabManager::NewStack()->SetHideTabWell(true)->SetSizeCoefficient(0.75f)->AddTab("Graph", ETabState::OpenedTab))
			->Split(FTabManager::NewStack()->SetHideTabWell(true)->SetSizeCoefficient(0.25f)->AddTab("Details", ETabState::OpenedTab)))
		->Split(FTabManager::NewStack()->SetHideTabWell(true)->SetSizeCoefficient(0.05f)->AddTab("Status", ETabState::OpenedTab)));
}

TSharedRef<SDockTab> FTaskEditor::SpawnGraphWidgetTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)[GraphWidget.ToSharedRef()];
}

TSharedRef<SDockTab> FTaskEditor::SpawnDetailsWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("DetailsTab", "Details"))
		.ShouldAutosize(false)
		[
			DetailsWidget->TakeWidget()
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> FTaskEditor::SpawnStatusWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("StatusTab", "Status"))
		.ShouldAutosize(true)
		[
			StatusWidget->TakeWidget()
		];
	return SpawnedTab;
}

void FTaskEditor::PostUndo(bool bSuccess)
{
	FAssetEditorBase::PostUndo(bSuccess);
	if (bSuccess && GraphWidget) GraphWidget->Rebuild();
}

void FTaskEditor::PostRedo(bool bSuccess)
{
	FAssetEditorBase::PostRedo(bSuccess);
	if (bSuccess && GraphWidget) GraphWidget->Rebuild();
}

FEditorModuleBase* FTaskEditor::GetEditorModule() const
{
	return &FTaskEditorModule::Get();
}

void FTaskEditor::OnBlueprintCompiled()
{
	FAssetEditorBase::OnBlueprintCompiled();

	if (GraphWidget) GraphWidget->Rebuild();
}

void FTaskEditor::RefreshDetails()
{
	if (DetailsWidget) DetailsWidget->Refresh();
}

void FTaskEditor::OnTaskPropertyChanged(const FPropertyChangedEvent& Event)
{
	if (GraphWidget) GraphWidget->Rebuild();
}

//////////////////////////////////////////////////////////////////////////
/// TaskBlueprintEditor
FTaskBlueprintEditor::FTaskBlueprintEditor()
{
	BlueprintClass = UTaskBlueprint::StaticClass();
	GraphSchemaClass = UTaskGraphSchema::StaticClass();
	
	ToolkitFName = FName("TaskBlueprintEditor");
	BaseToolkitName = LOCTEXT("TaskBlueprintEditorAppLabel", "Task Blueprint Editor");
	ToolkitNameFormat = LOCTEXT("TaskBlueprintEditorToolkitName", "{ObjectName}{DirtyState}");

	WorldCentricTabPrefix = TEXT("TaskBlueprintEditor");
	WorldCentricTabColorScale = FLinearColor::White;
}

#undef LOCTEXT_NAMESPACE
