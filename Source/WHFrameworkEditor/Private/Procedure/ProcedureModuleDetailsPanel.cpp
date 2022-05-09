// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProcedureModuleDetailsPanel.h"

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
#include "Procedure/ProcedureModule.h"
#include "Procedure/Base/ProcedureBase.h"

FProcedureModuleDetailsPanel::FProcedureModuleDetailsPanel() {}

TSharedRef<IDetailCustomization> FProcedureModuleDetailsPanel::MakeInstance()
{
	return MakeShared<FProcedureModuleDetailsPanel>();
}

void FProcedureModuleDetailsPanel::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	SelectedObjectsList = DetailLayoutBuilder.GetSelectedObjects();

	IDetailCategoryBuilder& ProcedureModuleCategory = DetailLayoutBuilder.EditCategory(FName("ProcedureModule"));

	TSharedRef<SWrapBox> ProcedureModuleActionBox = SNew(SWrapBox).UseAllottedWidth(true);
	ProcedureModuleActionBox->AddSlot()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Open Procedure Editor")))
			.OnClicked(FOnClicked::CreateSP(this, &FProcedureModuleDetailsPanel::OnOnClickOpenProcedureEditorButton))
		]
	];

	ProcedureModuleCategory.AddCustomRow(FText::GetEmpty())
		.ValueContent()
		[
			ProcedureModuleActionBox
		];
}

FReply FProcedureModuleDetailsPanel::OnOnClickOpenProcedureEditorButton()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FName("ProcedureEditor"));

	return FReply::Handled();
}
