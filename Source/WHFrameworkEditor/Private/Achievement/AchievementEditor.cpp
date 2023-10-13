// Copyright 2021 Sam Carey. All Rights Reserved.

#include "Achievement/AchievementEditor.h"

IMPLEMENTATION_EDITOR_MODULE(FAchievementEditor)

void FAchievementEditor::StartupModule()
{
	//Register achievement combo Box pin Factory
	BlueprintGraphPanelPinFactory = MakeShareable(new FAchievementPinFactory());
	FEdGraphUtilities::RegisterVisualPinFactory(BlueprintGraphPanelPinFactory);
}

void FAchievementEditor::ShutdownModule()
{
	//Unregister achievement combo Box pin Factory
	FEdGraphUtilities::UnregisterVisualPinFactory(BlueprintGraphPanelPinFactory);
}
