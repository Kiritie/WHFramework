// Copyright 2021 Sam Carey. All Rights Reserved.
#pragma once

#include "IAssetTypeActions.h"
#include "ISettingsModule.h"
#include "Main/MainEditorTypes.h"

class FModuleEditorBase
{
public:
	virtual ~FModuleEditorBase();

public:
	virtual void OnInitialize();

	virtual void OnTermination();

public:
	virtual void RegisterSettings(ISettingsModule* InSettingsModule);

	virtual void UnRegisterSettings();

	virtual bool HandleSettingsSaved();

	virtual void RegisterAssetTypeAction(class IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);

	virtual void RegisterCommands(const TSharedPtr<FUICommandList>& InCommands);
};
