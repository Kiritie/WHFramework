// Copyright Epic Games, Inc. All Rights Reserved.

#include "Procedure/ProcedureEditor.h"
#include "Procedure/Slate/SProcedureGraphWidget.h"
#include "Procedure/Graph/ProcedureAssetGraphNode.h"
#include "EdGraphUtilities.h"

#include "Editor.h"
#include "WHFrameworkSlateStatics.h"
#include "Procedure/ProcedureModule.h"
#include "Procedure/Base/ProcedureAsset.h"
#include "Procedure/Base/ProcedureBlueprint.h"
#include "Procedure/Blueprint/ProcedureBlueprintActions.h"
#include "Procedure/Blueprint/ProcedureGraphSchema.h"
#include "Procedure/Customization/ProcedureCustomization.h"
#include "Procedure/ProcedureEditorTypes.h"
#include "PropertyEditorModule.h"
#include "Widgets/Text/STextBlock.h"
#include "Procedure/Base/ProcedureBase.h"
#include "IDetailsView.h"
#include "UObject/UObjectHash.h"
#include "Procedure/Graph/ProcedureGraphOperations.h"
#include "Procedure/ProcedureEditorToolset.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "Misc/CoreDelegates.h"


#define LOCTEXT_NAMESPACE "FProcedureEditor"

//////////////////////////////////////////////////////////////////////////
/// ProcedureEditorModule
IMPLEMENTATION_EDITOR_MODULE(FProcedureEditorModule)

FProcedureEditorModule::FProcedureEditorModule()
{
	AppIdentifier = FName("ProcedureEditorApp");
}

void FProcedureEditorModule::StartupModule()
{
	FProcedureEditorCommands::Register();
	GraphNodeFactory = CreateProcedureGraphNodeFactory();
	FEdGraphUtilities::RegisterVisualNodeFactory(GraphNodeFactory);
	if (!IsRunningCommandlet())
	{
		if (UToolsetRegistry::IsAvailable())
		{
			RegisterToolsets();
		}
		else
		{
			FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddRaw(this, &FProcedureEditorModule::RegisterToolsets);
		}
	}
}

void FProcedureEditorModule::ShutdownModule()
{
	FCoreDelegates::OnAllModuleLoadingPhasesComplete.RemoveAll(this);
	if (UToolsetRegistry::IsAvailable())
	{
		UToolsetRegistry::UnregisterToolsetClass(UProcedureEditorToolset::StaticClass());
	}
	FEdGraphUtilities::UnregisterVisualNodeFactory(GraphNodeFactory);
	GraphNodeFactory.Reset();
	FProcedureEditorCommands::Unregister();
}

void FProcedureEditorModule::RegisterToolsets()
{
	UToolsetRegistry::RegisterToolsetClass(UProcedureEditorToolset::StaticClass());
}

void FProcedureEditorModule::RegisterCommands(const TSharedPtr<FUICommandList>& Commands)
{
	Commands->MapAction(
		FProcedureEditorCommands::Get().OpenProcedureEditorWindow,
		FExecuteAction::CreateRaw(this, &FProcedureEditorModule::OnClickedProcedureEditorButton),
		FCanExecuteAction());
}

void FProcedureEditorModule::RegisterMenus(const TSharedPtr<FUICommandList>& Commands)
{
	AddWindowMenu(FProcedureEditorCommands::Get().OpenProcedureEditorWindow, Commands);
	
	AddToolbarMenu(FProcedureEditorCommands::Get().OpenProcedureEditorWindow, Commands);
}

void FProcedureEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, EAssetTypeCategories::Type AssetCategory, TArray<TSharedPtr<IAssetTypeActions>>& AssetTypeActions)
{
	const TSharedRef<IAssetTypeActions> AssetAction = MakeShareable(new FProcedureBlueprintActions(AssetCategory));
	AssetTools.RegisterAssetTypeActions(AssetAction);
	AssetTypeActions.Add(AssetAction);
}

void FProcedureEditorModule::RegisterCustomization(FPropertyEditorModule& PropertyEditor)
{
	FEditorModuleBase::RegisterCustomization(PropertyEditor);
	
	PropertyEditor.RegisterCustomClassLayout(FName("ProcedureBase"), FOnGetDetailCustomizationInstance::CreateStatic(&FProcedureCustomization::MakeInstance));
}

void FProcedureEditorModule::UnRegisterCustomization(FPropertyEditorModule& PropertyEditor)
{
	PropertyEditor.UnregisterCustomClassLayout(FName("ProcedureBase"));
}

TSharedRef<FProcedureEditor> FProcedureEditorModule::CreateProcedureEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UProcedureAsset* Procedure)
{
	TSharedRef< FProcedureEditor > NewProcedureEditor( new FProcedureEditor() );
	NewProcedureEditor->InitAssetEditorBase( Mode, InitToolkitHost, Procedure );
	return NewProcedureEditor;
}

void FProcedureEditorModule::OnClickedProcedureEditorButton()
{
    if (const UProcedureModule* Module = UProcedureModule::GetPtr(!UCommonModuleStatics::IsPlaying()))
    {
        if (UProcedureAsset* Asset = Module->GetDefaultAsset())
        {
            GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Asset);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
/// ProcedureEditor
FProcedureEditor::FProcedureEditor()
{
	ToolkitFName = FName("ProcedureEditor");
	BaseToolkitName =  LOCTEXT("AppLabel", "Procedure Editor");
	MenuCategory = LOCTEXT("ProcedureEditor", "Procedure Editor");
	DefaultLayoutName = FName("ProcedureEditor_ContextGraphLayout_v2");
	WorldCentricTabPrefix =  LOCTEXT("WorldCentricTabPrefix", "Procedure ").ToString();
	WorldCentricTabColorScale =  FLinearColor( 0.0f, 0.0f, 0.2f, 0.5f );
}

FProcedureEditor::~FProcedureEditor()
{
    if (GEditor)
    {
        GEditor->UnregisterForUndo(this);
    }
}

void FProcedureEditor::InitAssetEditorBase(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* Asset)
{
    Asset->SetFlags(RF_Transactional);
    TArray<UObject*> Objects;
    GetObjectsWithOuter(Asset, Objects, true);
    for (UObject* Object : Objects)
    {
        Object->SetFlags(RF_Transactional);
    }
    GEditor->RegisterForUndo(this);
	FAssetEditorBase::InitAssetEditorBase(Mode, InitToolkitHost, Asset);
}


void FProcedureEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
    FAssetEditorBase::RegisterTabSpawners(InTabManager);
    FDetailsViewArgs DetailsArgs;
    DetailsArgs.bHideSelectionTip = true;
    DetailsArgs.bUpdatesFromSelection = false;
    DetailsArgs.NotifyHook = this;
    DetailsView = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsArgs);
    SAssignNew(GraphWidget, SProcedureGraphWidget).ProcedureEditor(SharedThis(this));
    GetToolkitCommands()->Append(GraphWidget->GetCommands().ToSharedRef());
    DetailsView->OnFinishedChangingProperties().AddSP(this, &FProcedureEditor::OnProcedurePropertyChanged);
    DetailsView->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateSP(GraphWidget.ToSharedRef(), &SProcedureGraphWidget::CanEdit));
    RefreshDetails();
    RegisterTrackedTabSpawner(InTabManager, "Graph", FOnSpawnTab::CreateSP(this, &FProcedureEditor::SpawnGraphWidgetTab))
        .SetDisplayName(NSLOCTEXT("ProcedureGraph", "GraphTab", "流程图"));
    RegisterTrackedTabSpawner(InTabManager, "Details", FOnSpawnTab::CreateSP(this, &FProcedureEditor::SpawnDetailsWidgetTab))
        .SetDisplayName(NSLOCTEXT("ProcedureGraph", "DetailsTab", "属性"));
    RegisterTrackedTabSpawner(InTabManager, "Status", FOnSpawnTab::CreateSP(this, &FProcedureEditor::SpawnStatusWidgetTab))
        .SetDisplayName(NSLOCTEXT("ProcedureGraph", "StatusTab", "编译状态"));
}

void FProcedureEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner("Graph");
	InTabManager->UnregisterTabSpawner("Details");
	InTabManager->UnregisterTabSpawner("Status");
}

TSharedRef<FTabManager::FLayout> FProcedureEditor::CreateDefaultLayout()
{
	return FTabManager::NewLayout(DefaultLayoutName)->AddArea(
		FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
		->Split(FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)->SetSizeCoefficient(0.95f)
			->Split(FTabManager::NewStack()->SetHideTabWell(true)->SetSizeCoefficient(0.75f)->AddTab("Graph", ETabState::OpenedTab))
			->Split(FTabManager::NewStack()->SetHideTabWell(true)->SetSizeCoefficient(0.25f)->AddTab("Details", ETabState::OpenedTab)))
		->Split(FTabManager::NewStack()->SetHideTabWell(true)->SetSizeCoefficient(0.05f)->AddTab("Status", ETabState::OpenedTab)));
}

TSharedRef<SDockTab> FProcedureEditor::SpawnGraphWidgetTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)[GraphWidget.ToSharedRef()];
}


TSharedRef<SDockTab> FProcedureEditor::SpawnDetailsWidgetTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)[DetailsView.ToSharedRef()];
}

TSharedRef<SDockTab> FProcedureEditor::SpawnStatusWidgetTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)[SNew(STextBlock).Text_Lambda([this]
    {
        const UProcedureAsset* Asset = GetEditingAsset<UProcedureAsset>();
        return Asset->bCompiled ? FText::Format(NSLOCTEXT("ProcedureGraph", "Compiled", "可达节点：{0} / {1}；断线不会自动跳过"),
            FText::AsNumber(Asset->CompiledNodes.Num()), FText::AsNumber(Asset->Nodes.Num())) : FText::FromString(Asset->CompileError);
    })];
}

void FProcedureEditor::PostUndo(bool bSuccess)
{
	FAssetEditorBase::PostUndo(bSuccess);
	if (bSuccess && GraphWidget) GraphWidget->Rebuild();
}

void FProcedureEditor::PostRedo(bool bSuccess)
{
	FAssetEditorBase::PostRedo(bSuccess);
	if (bSuccess && GraphWidget) GraphWidget->Rebuild();
}

FEditorModuleBase* FProcedureEditor::GetEditorModule() const
{
	return &FProcedureEditorModule::Get();
}

void FProcedureEditor::OnBlueprintCompiled()
{
	FAssetEditorBase::OnBlueprintCompiled();

	if (GraphWidget) GraphWidget->Rebuild();
}


void FProcedureEditor::RefreshDetails()
{
    if (!DetailsView || !GraphWidget)
    {
        return;
    }
    UProcedureAsset* Asset = GetEditingAsset<UProcedureAsset>();
    TArray<UObject*> Objects;
    for (const FGuid& Id : GraphWidget->GetSelection())
    {
        if (UProcedureBase* Node = Asset->FindNode(Id))
        {
            Objects.Add(Node);
        }
    }
    if (Objects.IsEmpty())
    {
        Objects.Add(Asset);
    }
    DetailsView->SetObjects(Objects, true);
}

void FProcedureEditor::NotifyPreChange(FProperty* PropertyAboutToChange)
{
    if (GraphWidget && GraphWidget->CanEdit())
    {
        FProcedureGraphOperations::Record(GraphWidget->GetGraph());
    }
}

void FProcedureEditor::NotifyPreChange(FEditPropertyChain* PropertyAboutToChange)
{
    NotifyPreChange(static_cast<FProperty*>(nullptr));
}

void FProcedureEditor::OnProcedurePropertyChanged(const FPropertyChangedEvent& Event)
{
    if (GraphWidget)
    {
        GetEditingAsset<UProcedureAsset>()->Recompile();
        GraphWidget->Rebuild();
    }
}

//////////////////////////////////////////////////////////////////////////
/// ProcedureBlueprintEditor
FProcedureBlueprintEditor::FProcedureBlueprintEditor()
{
	BlueprintClass = UProcedureBlueprint::StaticClass();
	GraphSchemaClass = UProcedureGraphSchema::StaticClass();
	
	ToolkitFName = FName("ProcedureBlueprintEditor");
	BaseToolkitName = LOCTEXT("ProcedureBlueprintEditorAppLabel", "Procedure Blueprint Editor");
	ToolkitNameFormat = LOCTEXT("ProcedureBlueprintEditorToolkitName", "{ObjectName}{DirtyState}");

	WorldCentricTabPrefix = TEXT("ProcedureBlueprintEditor");
	WorldCentricTabColorScale = FLinearColor::White;
}

#undef LOCTEXT_NAMESPACE
