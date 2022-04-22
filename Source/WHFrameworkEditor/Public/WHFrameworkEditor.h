// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IAssetTypeActions.h"

#include "Modules/ModuleManager.h"

class SProcedureEditorWidget;
class FToolBarBuilder;
class FMenuBuilder;

class FWHFrameworkEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();

	void RegisterSettings();

	void UnRegisterSettings();

	bool HandleSettingsSaved();

	void RegisterAssetTypeAction(class IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);
	
private:
	TSharedPtr<class FUICommandList> PluginCommands;

	TArray< TSharedPtr<IAssetTypeActions> > CreatedAssetTypeActions;

	//////////////////////////////////////////////////////////////////////////
	/// ProcedureEditor
private:
	void OnClickedProcedureEditorButton();
	
	TSharedRef<class SDockTab> OnSpawnProcedureEditorTab(const class FSpawnTabArgs& SpawnTabArgs);

private:
	TSharedPtr<SProcedureEditorWidget> ProcedureEditorWidget;
};
