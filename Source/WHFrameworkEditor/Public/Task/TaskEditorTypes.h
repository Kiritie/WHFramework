// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TaskEditor.h"
#include "WHFrameworkEditorStyle.h"
#include "Styling/SlateStyle.h"
#include "TaskEditorTypes.generated.h"

FString GTaskEditorIni;

//////////////////////////////////////////////////////////////////////////
// ClassFilter
class FTaskClassFilter : public FClassViewerFilterBase
{
public:
	FTaskClassFilter();

	TWeakPtr<FTaskEditor> TaskEditor;

public:
	virtual bool IsClassAllowed(UClass* InClass) override;
};

//////////////////////////////////////////////////////////////////////////
// Commands
class FTaskEditorCommands : public TCommands<FTaskEditorCommands>
{	  
public:
	FTaskEditorCommands()
		: TCommands<FTaskEditorCommands>(TEXT("TaskEditor"),
			NSLOCTEXT("TaskEditor", "Task Editor", "Task Editor Commands"),
			NAME_None, FWHFrameworkEditorStyle::GetStyleSetName())
	{}
	
public:
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenTaskEditorWindow;
};

//////////////////////////////////////////////////////////////////////////
// EditorSettings
UCLASS(config = TaskEditor, configdonotcheckdefaults)
class WHFRAMEWORKEDITOR_API UTaskEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	UTaskEditorSettings();

public:
	UPROPERTY(Config, EditAnywhere, Category = "List")
	bool bDefaultIsDefaults;

	UPROPERTY(Config, EditAnywhere, Category = "List")
	bool bDefaultIsEditing;
};
