// Copyright Epic Games, Inc. All Rights Reserved.

#include "Task/TaskDetailsPanel.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "UObject/UnrealType.h"

#include "DetailLayoutBuilder.h"
#include "IDetailsView.h"

#include "ScopedTransaction.h"

FTaskDetailsPanel::FTaskDetailsPanel() {}

TSharedRef<IDetailCustomization> FTaskDetailsPanel::MakeInstance() { return MakeShared<FTaskDetailsPanel>(); }

void FTaskDetailsPanel::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	SelectedObjectsList = DetailLayoutBuilder.GetSelectedObjects();
}
