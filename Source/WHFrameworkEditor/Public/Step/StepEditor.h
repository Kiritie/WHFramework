// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/CommonEditorTypes.h"
#include "Common/Asset/AssetEditorBase.h"
#include "Common/Module/EditorModuleBase.h"
#include "Common/Blueprint/BlueprintEditorBase.h"

class FStepEditor;
class UStepAsset;

//////////////////////////////////////////////////////////////////////////
/// StepEditorModule
class FStepEditorModule : public FEditorModuleBase
{
	GENERATED_EDITOR_MODULE(FStepEditorModule)

public:
	FStepEditorModule();
	
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

	virtual void RegisterCustomClassLayout(FPropertyEditorModule& PropertyEditor) override;

	virtual void UnRegisterCustomClassLayout(FPropertyEditorModule& PropertyEditor) override;
	
public:
	TSharedRef<FStepEditor> CreateStepEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UStepAsset* Step );

private:
	void OnClickedStepEditorButton();
};

//////////////////////////////////////////////////////////////////////////
/// StepEditor
class FStepEditor : public FAssetEditorBase
{
public:
	FStepEditor();

	~FStepEditor();

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
	TSharedPtr<class SStepListWidget> ListWidget;

	TSharedPtr<class SStepDetailsWidget> DetailsWidget;

	TSharedPtr<class SStepStatusWidget> StatusWidget;
};

//////////////////////////////////////////////////////////////////////////
// FStepBlueprintEditor
class FStepBlueprintEditor : public FBlueprintEditorBase
{
public:
	FStepBlueprintEditor();
};
