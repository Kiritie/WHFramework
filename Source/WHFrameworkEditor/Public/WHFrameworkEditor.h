// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IAssetTypeActions.h"

#include "Modules/ModuleManager.h"

struct FClassPickerDefaults;
class FToolBarBuilder;
class FMenuBuilder;

class FWHFrameworkEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public:
	void StartupEditorModules();

	void ShutdownEditorModules();

	void RegisterMenus();

	void UnRegisterMenus();

protected:
	void OnPostEngineInit();

	void OnBeginPIE(bool bIsSimulating);

	void OnEndPIE(bool bIsSimulating);

protected:
	const TArray<FClassPickerDefaults>& GetNewAssetDefaultClasses();

	void OnBlueprintCompiled();

	void OnLevelViewportClientListChanged();

private:
	TArray<FClassPickerDefaults> DefaultClassPickers;

	TSharedPtr<class FUICommandList> PluginCommands;

	TArray< TSharedPtr<IAssetTypeActions> > CreatedAssetTypeActions;

	FDelegateHandle BeginPIEDelegateHandle;

	FDelegateHandle EndPIEDelegateHandle;
};
