// Copyright 2021 Sam Carey. All Rights Reserved.
#pragma once

#include "Main/MainEditorTypes.h"

class FModuleEditorBase
{
public:
	virtual ~FModuleEditorBase();

public:
	/* Called when the module is loaded */
	virtual void StartupModule();

	/* Called when the module is unloaded */
	virtual void ShutdownModule();

	virtual void RegisterCommands(const TSharedPtr<FUICommandList>& InCommands);
};