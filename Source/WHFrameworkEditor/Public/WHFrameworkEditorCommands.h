// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WHFrameworkEditorStyle.h"
#include "Framework/Commands/Commands.h"

class FWHFrameworkEditorCommands : public TCommands<FWHFrameworkEditorCommands>
{
public:

	FWHFrameworkEditorCommands()
		: TCommands<FWHFrameworkEditorCommands>(TEXT("WHFrameworkEditor"), NSLOCTEXT("Contexts", "WHFrameworkEditor", "WHFrameworkEditor Plugin"), NAME_None, FWHFrameworkEditorStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenProcedureEditorWindow;
};