// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IAssetTypeActions.h"

#include "Modules/ModuleManager.h"

class SStepEditorWidget;
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

	FDelegateHandle BeginPIEDelegateHandle;

	FDelegateHandle EndPIEDelegateHandle;

public:
	bool bPlaying;

private:
	void OnBeginPIE(bool bIsSimulating);

	void OnEndPIE(bool bIsSimulating);

	//////////////////////////////////////////////////////////////////////////
	/// ProcedureEditor
private:
	void OnClickedProcedureEditorButton();
	
	TSharedRef<class SDockTab> OnSpawnProcedureEditorTab(const class FSpawnTabArgs& SpawnTabArgs);

private:
	TSharedPtr<SProcedureEditorWidget> ProcedureEditorWidget;

	//////////////////////////////////////////////////////////////////////////
	/// StepEditor
private:
	void OnClickedStepEditorButton();
	
	TSharedRef<class SDockTab> OnSpawnStepEditorTab(const class FSpawnTabArgs& SpawnTabArgs);

private:
	TSharedPtr<SStepEditorWidget> StepEditorWidget;
};
