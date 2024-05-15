// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "WHFrameworkEditorStyle.h"
#include "WHFrameworkEditorTypes.h"
#include "AssetEditorTypes.generated.h"

class UAssetModifierBase;

FString GAssetEditorIni;

//////////////////////////////////////////////////////////////////////////
// Commands
class FAssetModifierEditorCommands : public TCommands<FAssetModifierEditorCommands>
{	  
public:
	FAssetModifierEditorCommands()
		: TCommands<FAssetModifierEditorCommands>(TEXT("AssetModifierEditor"),
			NSLOCTEXT("AssetModifierEditor", "Asset Modifier Editor", "Asset Modifier Editor Commands"),
			NAME_None, FWHFrameworkEditorStyle::GetStyleSetName())
	{}
	
public:
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenAssetModifierEditorWindow;
};

//////////////////////////////////////////////////////////////////////////
// ClassFilter
class FAssetModifierClassFilter : public FClassViewerFilterBase
{
public:
	FAssetModifierClassFilter();

public:
	virtual bool IsClassAllowed(UClass* InClass) override;
};

//////////////////////////////////////////////////////////////////////////
// EditorSettings
UCLASS(config = AssetModifierEditor, configdonotcheckdefaults)
class WHFRAMEWORKEDITOR_API UAssetModifierEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	UAssetModifierEditorSettings();
};
