// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StepEditor.h"
#include "WHFrameworkEditorStyle.h"
#include "Styling/SlateStyle.h"
#include "StepEditorTypes.generated.h"

FString GStepEditorIni;

//////////////////////////////////////////////////////////////////////////
// ClassFilter
class FStepClassFilter : public FAssetClassFilterBase
{
public:
	FStepClassFilter();

	TWeakPtr<FStepEditor> StepEditor;

private:
	virtual bool IsClassAllowed(const UClass* InClass) override;
};

//////////////////////////////////////////////////////////////////////////
// Commands
class FStepEditorCommands : public TCommands<FStepEditorCommands>
{	  
public:
	FStepEditorCommands()
		: TCommands<FStepEditorCommands>(TEXT("StepEditor"),
			NSLOCTEXT("StepEditor", "Step Editor", "Step Editor Commands"),
			NAME_None, FWHFrameworkEditorStyle::GetStyleSetName())
	{}
	
public:
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenStepEditorWindow;
};

//////////////////////////////////////////////////////////////////////////
// EditorSettings
UCLASS(config = StepEditor, configdonotcheckdefaults)
class WHFRAMEWORKEDITOR_API UStepEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	UStepEditorSettings();

public:
	UPROPERTY(Config, EditAnywhere, Category = "List")
	bool bDefaultIsMultiMode;

	UPROPERTY(Config, EditAnywhere, Category = "List")
	bool bDefaultIsEditMode;
};
