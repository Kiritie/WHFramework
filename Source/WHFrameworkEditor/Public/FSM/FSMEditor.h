// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/CommonEditorTypes.h"
#include "Common/Module/EditorModuleBase.h"
#include "Common/Blueprint/BlueprintEditorBase.h"

//////////////////////////////////////////////////////////////////////////
// FFSMEditorModule
class FFSMEditorModule : public FEditorModuleBase
{
	GENERATED_EDITOR_MODULE(FFSMEditorModule)

public:
	FFSMEditorModule();
	
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
};

//////////////////////////////////////////////////////////////////////////
// FFiniteStateBlueprintEditor
class FFiniteStateBlueprintEditor : public FBlueprintEditorBase
{
public:
	FFiniteStateBlueprintEditor();
};