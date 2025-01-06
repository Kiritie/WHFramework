// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WHFrameworkEditorStyle.h"

//////////////////////////////////////////////////////////////////////////
// Commands
class FWHFrameworkEditorCommands : public TCommands<FWHFrameworkEditorCommands>
{
public:

	FWHFrameworkEditorCommands()
		: TCommands<FWHFrameworkEditorCommands>(FName("WHFrameworkEditor"), NSLOCTEXT("Contexts", "WHFrameworkEditor", "WHFrameworkEditor Plugin"), NAME_None, FWHFrameworkEditorStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

	static void MapActions(TSharedRef<FUICommandList>& ToolkitCommands);
};

//////////////////////////////////////////////////////////////////////////
// Callbacks
class WHFRAMEWORKEDITOR_API FWHFrameworkEditorDelegates
{
public:
};

//////////////////////////////////////////////////////////////////////////
// Delegates
class WHFRAMEWORKEDITOR_API FWHFrameworkEditorCallbacks
{
public:
};
