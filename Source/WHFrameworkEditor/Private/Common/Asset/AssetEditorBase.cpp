// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/Asset/AssetEditorBase.h"

#include "Editor.h"
#include "Procedure/Widget/SProcedureListWidget.h"

#define LOCTEXT_NAMESPACE "FAssetEditorBase"

FAssetEditorBase::FAssetEditorBase()
{
	ToolkitFName = FName("AssetEditorBase");
	BaseToolkitName =  LOCTEXT("AppLabel", "Asset Editor Base");
	MenuCategory = LOCTEXT("AssetEditorBase", "Asset Editor Base");
	DefaultLayoutName = FName("AssetEditorBase_Layout");
	WorldCentricTabPrefix =  LOCTEXT("WorldCentricTabPrefix", "Asset Editor Base ").ToString();
	WorldCentricTabColorScale =  FLinearColor( 0.0f, 0.0f, 0.2f, 0.5f );

	EditingAsset = nullptr;
}

FAssetEditorBase::~FAssetEditorBase()
{
	GEditor->UnregisterForUndo(this);

	if(OnBlueprintCompiledHandle.IsValid())
	{
		GEditor->OnBlueprintCompiled().Remove(OnBlueprintCompiledHandle);
	}
}

void FAssetEditorBase::PostInitAssetEditor()
{
	FAssetEditorToolkit::PostInitAssetEditor();
	
	OnBlueprintCompiledHandle = GEditor->OnBlueprintCompiled().AddRaw(this, &FAssetEditorBase::OnBlueprintCompiled);
}

void FAssetEditorBase::InitAssetEditorBase( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UObject* Asset )
{
	EditingAsset = Asset;
	
	const TSharedRef<FTabManager::FLayout> DefaultLayout = CreateDefaultLayout();

	InitAssetEditor(Mode, InitToolkitHost, GetEditorModule()->AppIdentifier, DefaultLayout, true, true, Asset);

	const TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateRaw(this, &FAssetEditorBase::ExtendToolbar )
	);

	AddToolbarExtender(ToolbarExtender);
	
	RegenerateMenusAndToolbars();
}

void FAssetEditorBase::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(MenuCategory);

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	// const TSharedRef<SDockTab> NomadTab = SNew(SDockTab).TabRole(ETabRole::MajorTab);
	// this->InTabManager = FGlobalTabmanager::Get()->NewTabManager(NomadTab);
}

void FAssetEditorBase::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

FTabSpawnerEntry& FAssetEditorBase::RegisterTrackedTabSpawner(const TSharedRef<FTabManager>& InTabManager, const FName& TabId, const FOnSpawnTab& OnSpawnTab)
{
	return InTabManager->RegisterTabSpawner(TabId, FOnSpawnTab::CreateLambda([this, OnSpawnTab](const FSpawnTabArgs& Args) -> TSharedRef<SDockTab>
		         {
		             return OnSpawnTab.Execute(Args);
		         }))
		         .SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

TSharedRef<FTabManager::FLayout> FAssetEditorBase::CreateDefaultLayout()
{
	const TSharedRef<FTabManager::FLayout> DefaultLayout = FTabManager::NewLayout(DefaultLayoutName);
	return DefaultLayout;
}

void FAssetEditorBase::ExtendToolbar(FToolBarBuilder& ToolbarBuilder)
{
	
}

void FAssetEditorBase::PostRegenerateMenusAndToolbars()
{
	
}

void FAssetEditorBase::PostUndo(bool bSuccess)
{
	
}

void FAssetEditorBase::PostRedo(bool bSuccess)
{
	
}

void FAssetEditorBase::OnBlueprintCompiled()
{
}

FEditorModuleBase* FAssetEditorBase::GetEditorModule() const
{
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
