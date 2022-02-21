// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Modules/ModuleManager.h"

class SProcedureMainWidget;
class FToolBarBuilder;
class FMenuBuilder;

class FWHProcedureEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** This function will be bound to Command (by default it will bring up plugin window) */
	void PluginButtonClicked();
	
private:
	void RegisterMenus();

	void RegisterSettings();

	void UnRegisterSettings();

	bool HandleSettingsSaved();

	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

private:
	TSharedPtr<class FUICommandList> PluginCommands;

	TSharedPtr<SProcedureMainWidget> ProcedureMainWidget;
};
