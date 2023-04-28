// Copyright Epic Games, Inc. All Rights Reserved.

#include "Task/TaskModuleDetailsPanel.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "UObject/UnrealType.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "IDetailsView.h"

#include "ScopedTransaction.h"
#include "WHFrameworkEditor.h"
#include "Task/TaskModule.h"
#include "Task/Base/TaskBase.h"

FTaskModuleDetailsPanel::FTaskModuleDetailsPanel() {}

TSharedRef<IDetailCustomization> FTaskModuleDetailsPanel::MakeInstance()
{
	return MakeShared<FTaskModuleDetailsPanel>();
}

void FTaskModuleDetailsPanel::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	SelectedObjectsList = DetailLayoutBuilder.GetSelectedObjects();

	IDetailCategoryBuilder& TaskModuleCategory = DetailLayoutBuilder.EditCategory(FName("TaskModule"));

	TSharedRef<SWrapBox> TaskModuleActionBox = SNew(SWrapBox).UseAllottedWidth(true);
	TaskModuleActionBox->AddSlot()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Open Task Editor")))
			.OnClicked(FOnClicked::CreateSP(this, &FTaskModuleDetailsPanel::OnClickOpenTaskEditorButton))
		]
	];

	TaskModuleCategory.AddCustomRow(FText::GetEmpty())
		.ValueContent()
		[
			TaskModuleActionBox
		];
}

FReply FTaskModuleDetailsPanel::OnClickOpenTaskEditorButton()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FName("TaskEditor"));

	return FReply::Handled();
}
