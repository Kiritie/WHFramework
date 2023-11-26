// Copyright Epic Games, Inc. All Rights Reserved.

#include "Procedure/ProcedureModuleDetailsPanel.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "UObject/UnrealType.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailsView.h"

#include "ScopedTransaction.h"
#include "Procedure/ProcedureModule.h"

FProcedureModuleDetailsPanel::FProcedureModuleDetailsPanel() {}

TSharedRef<IDetailCustomization> FProcedureModuleDetailsPanel::MakeInstance()
{
	return MakeShared<FProcedureModuleDetailsPanel>();
}

void FProcedureModuleDetailsPanel::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	SelectedObjectsList = DetailLayoutBuilder.GetSelectedObjects();

	TSharedRef<IPropertyHandle> ModuleClassesProperty = DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureModule, bAutoSwitchFirst));

	DetailLayoutBuilder.AddCustomRowToCategory(ModuleClassesProperty, FText::GetEmpty())
		.WholeRowContent()
		[
			SNew(SWrapBox)
			.UseAllottedWidth(true)
			+SWrapBox::Slot()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Open Procedure Editor")))
				.OnClicked(FOnClicked::CreateSP(this, &FProcedureModuleDetailsPanel::OnOnClickOpenProcedureEditorButton))
			]
			+SWrapBox::Slot()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Clear All Procedure")))
				.OnClicked(FOnClicked::CreateSP(this, &FProcedureModuleDetailsPanel::OnOnClickClearAllProcedure))
			]
		];
}

FReply FProcedureModuleDetailsPanel::OnOnClickOpenProcedureEditorButton()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FName("ProcedureEditor"));

	return FReply::Handled();
}

FReply FProcedureModuleDetailsPanel::OnOnClickClearAllProcedure()
{
	for(auto Iter : SelectedObjectsList)
	{
		if(UProcedureModule* ProcedureModule = Cast<UProcedureModule>(Iter))
		{
			ProcedureModule->ClearAllProcedure();
		}
	}
	return FReply::Handled();
}
