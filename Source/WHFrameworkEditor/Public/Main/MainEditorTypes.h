// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "MainEditor.h"
#include "WHFrameworkEditorStyle.h"
#include "Common/CommonEditorTypes.h"
#include "MainEditorTypes.generated.h"

class AMainModule;

FString GModuleEditorIni;

//////////////////////////////////////////////////////////////////////////
// ClassFilter
class FModuleClassFilter : public FAssetClassFilterBase
{
public:
	FModuleClassFilter();

	AMainModule* MainModule;

private:
	virtual bool IsClassAllowed(const UClass* InClass) override;
};

//////////////////////////////////////////////////////////////////////////
// Commands
class FModuleEditorCommands : public TCommands<FModuleEditorCommands>
{	  
public:
	FModuleEditorCommands()
		: TCommands<FModuleEditorCommands>(TEXT("ModuleEditor"),
			NSLOCTEXT("ModuleEditor", "Module Editor", "Module Editor Commands"),
			NAME_None, FWHFrameworkEditorStyle::GetStyleSetName())
	{}
	
public:
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenModuleEditorWindow;
	
	TSharedPtr< FUICommandInfo > Save;
};

//////////////////////////////////////////////////////////////////////////
// EditorSettings
UCLASS(config = ModuleEditor, configdonotcheckdefaults)
class WHFRAMEWORKEDITOR_API UModuleEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	UModuleEditorSettings();

public:
	UPROPERTY(Config, EditAnywhere, Category = "List")
	bool bDefaultIsMultiMode;

	UPROPERTY(Config, EditAnywhere, Category = "List")
	bool bDefaultIsEditMode;
};
