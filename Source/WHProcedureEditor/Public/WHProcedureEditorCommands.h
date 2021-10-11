// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "WHProcedureEditorStyle.h"

class FWHProcedureEditorCommands : public TCommands<FWHProcedureEditorCommands>
{
public:

	FWHProcedureEditorCommands()
		: TCommands<FWHProcedureEditorCommands>(TEXT("WHProcedureEditor"), NSLOCTEXT("Contexts", "WHProcedureEditor", "WHProcedureEditor Plugin"), NAME_None, FWHProcedureEditorStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};