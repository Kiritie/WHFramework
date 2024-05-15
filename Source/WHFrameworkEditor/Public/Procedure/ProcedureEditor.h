// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WHFrameworkEditorTypes.h"
#include "Common/Asset/AssetEditorBase.h"
#include "Main/Module/EditorModuleBase.h"
#include "Common/Blueprint/BlueprintEditorBase.h"

class FProcedureEditor;
class UProcedureAsset;

//////////////////////////////////////////////////////////////////////////
/// ProcedureEditorModule
class FProcedureEditorModule : public FEditorModuleBase
{
	GENERATED_EDITOR_MODULE(FProcedureEditorModule)

public:
	FProcedureEditorModule();
	
public:
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

public:
	virtual void RegisterSettings(ISettingsModule* SettingsModule) override;

	virtual void UnRegisterSettings(ISettingsModule* SettingsModule) override;

	virtual bool HandleSettingsSaved() override;
	
	virtual void RegisterCommands(const TSharedPtr<FUICommandList>& Commands) override;

	virtual void RegisterMenus(const TSharedPtr<FUICommandList>& Commands) override;

	virtual void RegisterAssetTypeAction(IAssetTools& AssetTools, EAssetTypeCategories::Type AssetCategory, TArray<TSharedPtr<IAssetTypeActions>>& AssetTypeActions) override;

	virtual void RegisterCustomization(FPropertyEditorModule& PropertyEditor) override;

	virtual void UnRegisterCustomization(FPropertyEditorModule& PropertyEditor) override;
	
public:
	TSharedRef<FProcedureEditor> CreateProcedureEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UProcedureAsset* Procedure );

private:
	void OnClickedProcedureEditorButton();
};

//////////////////////////////////////////////////////////////////////////
/// ProcedureEditor
class FProcedureEditor : public FAssetEditorBase
{
public:
	FProcedureEditor();

	~FProcedureEditor();

public:
	virtual void InitAssetEditorBase(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* Asset) override;
	
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;

	virtual TSharedRef<FTabManager::FLayout> CreateDefaultLayout() override;

	virtual void ExtendToolbar(FToolBarBuilder& ToolbarBuilder) override;

	virtual void PostUndo(bool bSuccess) override;
	
	virtual void PostRedo(bool bSuccess) override;

public:
	virtual FEditorModuleBase* GetEditorModule() const override;

protected:
	TSharedRef<SDockTab> SpawnListWidgetTab(const FSpawnTabArgs& Args);

	TSharedRef<SDockTab> SpawnDetailsWidgetTab(const FSpawnTabArgs& Args);

	TSharedRef<SDockTab> SpawnStatusWidgetTab(const FSpawnTabArgs& Args);

protected:
	virtual void OnBlueprintCompiled() override;

	virtual void OnDefaultsToggled();

	virtual void OnEditingToggled();
	
	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<class SProcedureListWidget> ListWidget;

	TSharedPtr<class SProcedureDetailsWidget> DetailsWidget;

	TSharedPtr<class SProcedureStatusWidget> StatusWidget;
};

//////////////////////////////////////////////////////////////////////////
// FProcedureBlueprintEditor
class FProcedureBlueprintEditor : public FBlueprintEditorBase
{
public:
	FProcedureBlueprintEditor();
};
