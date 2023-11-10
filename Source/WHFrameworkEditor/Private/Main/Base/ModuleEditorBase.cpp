// Copyright 2021 Sam Carey. All Rights Reserved.

#include "Main/Base/ModuleEditorBase.h"

FModuleEditorBase::~FModuleEditorBase()
{
}

void FModuleEditorBase::OnInitialize()
{
}

void FModuleEditorBase::OnTermination()
{
}

void FModuleEditorBase::RegisterSettings(ISettingsModule* InSettingsModule)
{
	
}

void FModuleEditorBase::UnRegisterSettings()
{
}

bool FModuleEditorBase::HandleSettingsSaved()
{
	return false;
}

void FModuleEditorBase::RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
}

void FModuleEditorBase::RegisterCommands(const TSharedPtr<FUICommandList>& InCommands)
{
}
