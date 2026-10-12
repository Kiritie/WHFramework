// Copyright Epic Games, Inc. All Rights Reserved.

#include "Step/StepEditor.h"
#include "Step/Slate/SStepGraphWidget.h"
#include "Step/Graph/StepAssetGraphNode.h"
#include "EdGraphUtilities.h"

#include "Editor.h"
#include "UObject/UObjectHash.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "WHFrameworkSlateStatics.h"
#include "Step/StepModule.h"
#include "Step/Base/StepAsset.h"
#include "Step/Base/StepBlueprint.h"
#include "Step/Blueprint/StepBlueprintActions.h"
#include "Step/Blueprint/StepGraphSchema.h"
#include "Step/Customization/StepCustomization.h"
#include "Step/StepEditorTypes.h"
#include "Step/Slate/SStepDetailsWidget.h"
#include "IDetailsView.h"
#include "Step/Slate/SStepStatusWidget.h"

#define LOCTEXT_NAMESPACE "FStepEditor"

//////////////////////////////////////////////////////////////////////////
/// StepEditorModule
IMPLEMENTATION_EDITOR_MODULE(FStepEditorModule)

FStepEditorModule::FStepEditorModule()
{
	AppIdentifier = FName("StepEditorApp");
}

void FStepEditorModule::StartupModule()
{
	FStepEditorCommands::Register();
	GraphNodeFactory = CreateStepGraphNodeFactory();
	FEdGraphUtilities::RegisterVisualNodeFactory(GraphNodeFactory);
}

void FStepEditorModule::ShutdownModule()
{
	FEdGraphUtilities::UnregisterVisualNodeFactory(GraphNodeFactory);
	GraphNodeFactory.Reset();
	FStepEditorCommands::Unregister();
}

void FStepEditorModule::RegisterCommands(const TSharedPtr<FUICommandList>& Commands)
{
	Commands->MapAction(
		FStepEditorCommands::Get().OpenStepEditorWindow,
		FExecuteAction::CreateRaw(this, &FStepEditorModule::OnClickedStepEditorButton),
		FCanExecuteAction());
}

void FStepEditorModule::RegisterMenus(const TSharedPtr<FUICommandList>& Commands)
{
	AddWindowMenu(FStepEditorCommands::Get().OpenStepEditorWindow, Commands);
	
	AddToolbarMenu(FStepEditorCommands::Get().OpenStepEditorWindow, Commands);
}

void FStepEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, EAssetTypeCategories::Type AssetCategory, TArray<TSharedPtr<IAssetTypeActions>>& AssetTypeActions)
{
	const TSharedRef<IAssetTypeActions> AssetAction = MakeShareable(new FStepBlueprintActions(AssetCategory));
	AssetTools.RegisterAssetTypeActions(AssetAction);
	AssetTypeActions.Add(AssetAction);
}

void FStepEditorModule::RegisterCustomization(FPropertyEditorModule& PropertyEditor)
{
	FEditorModuleBase::RegisterCustomization(PropertyEditor);
	
	PropertyEditor.RegisterCustomClassLayout(FName("StepBase"), FOnGetDetailCustomizationInstance::CreateStatic(&FStepCustomization::MakeInstance));
}

void FStepEditorModule::UnRegisterCustomization(FPropertyEditorModule& PropertyEditor)
{
	PropertyEditor.UnregisterCustomClassLayout(FName("StepBase"));
}

TSharedRef<FStepEditor> FStepEditorModule::CreateStepEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UStepAsset* Step)
{
	TSharedRef< FStepEditor > NewStepEditor( new FStepEditor() );
	NewStepEditor->InitAssetEditorBase( Mode, InitToolkitHost, Step );
	return NewStepEditor;
}

void FStepEditorModule::OnClickedStepEditorButton()
{
	if(const UStepModule* Module = UStepModule::GetPtr(!UCommonModuleStatics::IsPlaying()))
	{
		UStepAsset* Asset = UCommonModuleStatics::IsPlaying() ? Module->GetCurrentAsset() : Module->GetDefaultAsset();
		if(Asset)
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Asset);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
/// StepEditor
FStepEditor::FStepEditor()
{
	ToolkitFName = FName("StepEditor");
	BaseToolkitName =  LOCTEXT("AppLabel", "Step Editor");
	MenuCategory = LOCTEXT("StepEditor", "Step Editor");
	DefaultLayoutName = FName("StepEditor_StartGraphLayout_v1");
	WorldCentricTabPrefix =  LOCTEXT("WorldCentricTabPrefix", "Step ").ToString();
	WorldCentricTabColorScale =  FLinearColor( 0.0f, 0.0f, 0.2f, 0.5f );
}

FStepEditor::~FStepEditor()
{
	if(GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}
}

void FStepEditor::InitAssetEditorBase(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* Asset)
{
	Asset->SetFlags(RF_Transactional);
	ForEachObjectWithOuter(Asset, [](UObject* Object)
	{
		Object->SetFlags(RF_Transactional);
	}, true);
	GEditor->RegisterForUndo(this);
	FAssetEditorBase::InitAssetEditorBase(Mode, InitToolkitHost, Asset);
}

void FStepEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorBase::RegisterTabSpawners(InTabManager);

	SAssignNewEd(DetailsWidget, SStepDetailsWidget, true)
		.StepEditor(SharedThis(this));

	SAssignNewEd(StatusWidget, SStepStatusWidget, true)
		.StepEditor(SharedThis(this));

	SAssignNew(GraphWidget, SStepGraphWidget).StepEditor(SharedThis(this));
	GetToolkitCommands()->Append(GraphWidget->GetCommands().ToSharedRef());
	DetailsWidget->DetailsView->OnFinishedChangingProperties().AddSP(this, &FStepEditor::OnStepPropertyChanged);
	DetailsWidget->DetailsView->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateSP(GraphWidget.ToSharedRef(), &SStepGraphWidget::CanEdit));
	RefreshDetails();
	RegisterTrackedTabSpawner(InTabManager, "Graph", FOnSpawnTab::CreateSP(this, &FStepEditor::SpawnGraphWidgetTab))
		.SetDisplayName(LOCTEXT("GraphTab", "Step Graph"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"));

	RegisterTrackedTabSpawner(InTabManager, "Details", FOnSpawnTab::CreateSP(this, &FStepEditor::SpawnDetailsWidgetTab))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	RegisterTrackedTabSpawner(InTabManager, "Status", FOnSpawnTab::CreateSP(this, &FStepEditor::SpawnStatusWidgetTab))
		.SetDisplayName(LOCTEXT("StatusTab", "Status"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.StatsViewer"));
}

void FStepEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner("Graph");
	InTabManager->UnregisterTabSpawner("Details");
	InTabManager->UnregisterTabSpawner("Status");
}

TSharedRef<FTabManager::FLayout> FStepEditor::CreateDefaultLayout()
{
	return FTabManager::NewLayout(DefaultLayoutName)->AddArea(
		FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
		->Split(FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)->SetSizeCoefficient(0.95f)
			->Split(FTabManager::NewStack()->SetHideTabWell(true)->SetSizeCoefficient(0.75f)->AddTab("Graph", ETabState::OpenedTab))
			->Split(FTabManager::NewStack()->SetHideTabWell(true)->SetSizeCoefficient(0.25f)->AddTab("Details", ETabState::OpenedTab)))
		->Split(FTabManager::NewStack()->SetHideTabWell(true)->SetSizeCoefficient(0.05f)->AddTab("Status", ETabState::OpenedTab)));
}

TSharedRef<SDockTab> FStepEditor::SpawnGraphWidgetTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)[GraphWidget.ToSharedRef()];
}

TSharedRef<SDockTab> FStepEditor::SpawnDetailsWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("DetailsTab", "Details"))
		.ShouldAutosize(false)
		[
			DetailsWidget->TakeWidget()
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> FStepEditor::SpawnStatusWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("StatusTab", "Status"))
		.ShouldAutosize(true)
		[
			StatusWidget->TakeWidget()
		];
	return SpawnedTab;
}

void FStepEditor::PostUndo(bool bSuccess)
{
	FAssetEditorBase::PostUndo(bSuccess);
	if(bSuccess && GraphWidget)
	{
		GraphWidget->Rebuild();
	}
}

void FStepEditor::PostRedo(bool bSuccess)
{
	FAssetEditorBase::PostRedo(bSuccess);
	if(bSuccess && GraphWidget)
	{
		GraphWidget->Rebuild();
	}
}

FEditorModuleBase* FStepEditor::GetEditorModule() const
{
	return &FStepEditorModule::Get();
}

void FStepEditor::OnBlueprintCompiled()
{
	FAssetEditorBase::OnBlueprintCompiled();

	if(GraphWidget)
	{
		GraphWidget->Rebuild();
	}
}

void FStepEditor::RefreshDetails()
{
	if(DetailsWidget)
	{
		DetailsWidget->Refresh();
	}
}

void FStepEditor::OnStepPropertyChanged(const FPropertyChangedEvent& Event)
{
	if(GraphWidget)
	{
		GraphWidget->Rebuild();
	}
}

//////////////////////////////////////////////////////////////////////////
/// StepBlueprintEditor
FStepBlueprintEditor::FStepBlueprintEditor()
{
	BlueprintClass = UStepBlueprint::StaticClass();
	GraphSchemaClass = UStepGraphSchema::StaticClass();
	
	ToolkitFName = FName("StepBlueprintEditor");
	BaseToolkitName = LOCTEXT("StepBlueprintEditorAppLabel", "Step Blueprint Editor");
	ToolkitNameFormat = LOCTEXT("StepBlueprintEditorToolkitName", "{ObjectName}{DirtyState}");

	WorldCentricTabPrefix = TEXT("StepBlueprintEditor");
	WorldCentricTabColorScale = FLinearColor::White;
}

#undef LOCTEXT_NAMESPACE
