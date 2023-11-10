// Copyright 2021 Sam Carey. All Rights Reserved.
#pragma once

#include "AchievementPinFactory.h"
#include "Main/Base/ModuleEditorBase.h"

class FAchievementEditor : public FModuleEditorBase
{
	GENERATED_EDITOR_MODULE(FAchievementEditor)
	
public:
	/* Called when the module is loaded */
	virtual void OnInitialize() override;

	/* Called when the module is unloaded */
	virtual void OnTermination() override;

protected:
	TSharedPtr<FAchievementPinFactory> BlueprintGraphPanelPinFactory;
};
