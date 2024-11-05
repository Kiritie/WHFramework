// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "MainEditor.h"
#include "WHFrameworkEditorStyle.h"
#include "WHFrameworkEditorTypes.h"
#include "MainEditorTypes.generated.h"

class UModuleBase;
class AMainModule;

FString GModuleEditorIni;

//////////////////////////////////////////////////////////////////////////
// ClassFilter
class FModuleClassFilter : public FClassViewerFilterBase
{
public:
	FModuleClassFilter();

	AMainModule* MainModule;

	TSubclassOf<UModuleBase> EditingModuleClass;

public:
	virtual bool IsClassAllowed(UClass* InClass) override;
};

//////////////////////////////////////////////////////////////////////////
// Commands
class FModuleEditorCommands : public TCommands<FModuleEditorCommands>
{	  
public:
	FModuleEditorCommands()
		: TCommands<FModuleEditorCommands>(FName("ModuleEditor"),
			NSLOCTEXT("ModuleEditor", "Module Editor", "Module Editor Commands"),
			NAME_None, FWHFrameworkEditorStyle::GetStyleSetName())
	{}
	
public:
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenModuleEditorWindow;
	
	TSharedPtr< FUICommandInfo > SaveModuleEditor;
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
	bool bDefaultIsDefaults;

	UPROPERTY(Config, EditAnywhere, Category = "List")
	bool bDefaultIsEditing;
};
