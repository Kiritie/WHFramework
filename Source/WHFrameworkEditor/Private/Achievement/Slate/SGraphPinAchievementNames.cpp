// Copyright 2021 Sam Carey. All Rights Reserved.

#include "Achievement/Slate/SGraphPinAchievementNames.h"

#include "Achievement/AchievementModule.h"
#include "Editor.h"

SGraphPinAchievementNames::SGraphPinAchievementNames()
{
}

void SGraphPinAchievementNames::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	RegenList();
	SGraphPinNameList::Construct(SGraphPinNameList::FArguments(), InGraphPinObj, NameList);

	//FString PreviousSelection = GraphPinObj->GetDefaultAsString();
	//FString a = PreviousSelection;
}

void SGraphPinAchievementNames::RegenList()
{
	NameList.Empty();
	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (UAchievementModule* Module = UAchievementModule::GetPtr(EditorWorld))
	{
		NameList = Module->GetComboBoxNames();
	}
}
