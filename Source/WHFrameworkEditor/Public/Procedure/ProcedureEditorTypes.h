// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProcedureEditor.h"
#include "WHFrameworkEditorStyle.h"
#include "Styling/SlateStyle.h"
#include "ProcedureEditorTypes.generated.h"

FString GProcedureEditorIni;

//////////////////////////////////////////////////////////////////////////
// ClassFilter
class FProcedureClassFilter : public FAssetClassFilterBase
{
public:
	FProcedureClassFilter();

	TWeakPtr<FProcedureEditor> ProcedureEditor;

private:
	virtual bool IsClassAllowed(const UClass* InClass) override;
};

//////////////////////////////////////////////////////////////////////////
// Commands
class FProcedureEditorCommands : public TCommands<FProcedureEditorCommands>
{	  
public:
	FProcedureEditorCommands()
		: TCommands<FProcedureEditorCommands>(TEXT("ProcedureEditor"),
			NSLOCTEXT("ProcedureEditor", "Procedure Editor", "Procedure Editor Commands"),
			NAME_None, FWHFrameworkEditorStyle::GetStyleSetName())
	{}
	
public:
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenProcedureEditorWindow;
};

//////////////////////////////////////////////////////////////////////////
// EditorSettings
UCLASS(config = ProcedureEditor, configdonotcheckdefaults)
class WHFRAMEWORKEDITOR_API UProcedureEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	UProcedureEditorSettings();

public:
	UPROPERTY(Config, EditAnywhere, Category = "List")
	bool bDefaultIsMultiMode;

	UPROPERTY(Config, EditAnywhere, Category = "List")
	bool bDefaultIsEditMode;
};
