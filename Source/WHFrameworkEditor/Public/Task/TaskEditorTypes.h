// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TaskEditor.h"
#include "WHFrameworkEditorStyle.h"
#include "Styling/SlateStyle.h"


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
		: TCommands<FTaskEditorCommands>(FName("TaskEditor"),
			NSLOCTEXT("TaskEditor", "Task Editor", "Task Editor Commands"),
			NAME_None, FWHFrameworkEditorStyle::GetStyleSetName())
	{}
	
public:
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenTaskEditorWindow;
};
