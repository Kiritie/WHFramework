// Copyright 2021 Sam Carey. All Rights Reserved.
#pragma once

#include "AssetTypeCategories.h"
#include "IAssetTypeActions.h"
#include "ISettingsModule.h"

class FEditorModuleBase
{
public:
	FEditorModuleBase();
	
	virtual ~FEditorModuleBase();

public:
	virtual void StartupModule();

	virtual void ShutdownModule();

public:
	virtual void RegisterSettings(ISettingsModule* SettingsModule);

	virtual void UnRegisterSettings(ISettingsModule* SettingsModule);

	virtual bool HandleSettingsSaved();

	virtual void RegisterCommands(const TSharedPtr<FUICommandList>& Commands);

	virtual void RegisterMenus(const TSharedPtr<FUICommandList>& Commands);

	virtual void RegisterAssetTypeAction(class IAssetTools& AssetTools, EAssetTypeCategories::Type AssetCategory, TArray<TSharedPtr<IAssetTypeActions>>& AssetTypeActions);

	virtual void RegisterCustomClassLayout(FPropertyEditorModule& PropertyEditor);

	virtual void UnRegisterCustomClassLayout(FPropertyEditorModule& PropertyEditor);

public:
	virtual void AddWindowMenu(const TSharedPtr<const FUICommandInfo>& Command, const TSharedPtr<const FUICommandList>& Commands);

	virtual void AddToolbarMenu(const TSharedPtr<const FUICommandInfo>& Command, const TSharedPtr<const FUICommandList>& Commands);

public:
	FName AppIdentifier;
};
