// Copyright Epic Games, Inc. All Rights Reserved.

#include "StepModuleDetailsPanel.h"

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
#include "Step/StepModule.h"
#include "Step/Base/StepBase.h"

FStepModuleDetailsPanel::FStepModuleDetailsPanel() {}

TSharedRef<IDetailCustomization> FStepModuleDetailsPanel::MakeInstance()
{
	return MakeShared<FStepModuleDetailsPanel>();
}

void FStepModuleDetailsPanel::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	SelectedObjectsList = DetailLayoutBuilder.GetSelectedObjects();

	IDetailCategoryBuilder& StepModuleCategory = DetailLayoutBuilder.EditCategory(FName("StepModule"));

	TSharedRef<SWrapBox> StepModuleActionBox = SNew(SWrapBox).UseAllottedWidth(true);
	StepModuleActionBox->AddSlot()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Open Step Editor")))
			.OnClicked(FOnClicked::CreateSP(this, &FStepModuleDetailsPanel::OnExecuteAction))
		]
	];

	StepModuleCategory.AddCustomRow(FText::GetEmpty())
		.ValueContent()
		[
			StepModuleActionBox
		];
}

FReply FStepModuleDetailsPanel::OnExecuteAction()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FName("StepEditor"));

	return FReply::Handled();
}
