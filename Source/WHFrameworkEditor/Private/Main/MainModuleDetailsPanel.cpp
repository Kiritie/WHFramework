// Copyright Epic Games, Inc. All Rights Reserved.

#include "Main/MainModuleDetailsPanel.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "UObject/UnrealType.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailsView.h"

#include "ScopedTransaction.h"
#include "Main/MainModule.h"
#include "Widgets/Layout/SWrapBox.h"

FMainModuleDetailsPanel::FMainModuleDetailsPanel() {}

TSharedRef<IDetailCustomization> FMainModuleDetailsPanel::MakeInstance()
{
	return MakeShared<FMainModuleDetailsPanel>();
}

void FMainModuleDetailsPanel::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	SelectedObjectsList = DetailLayoutBuilder.GetSelectedObjects();

	TSharedRef<IPropertyHandle> ModuleClassesProperty = DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(AMainModule, Modules));

	DetailLayoutBuilder.AddCustomRowToCategory(ModuleClassesProperty, FText::GetEmpty())
		.WholeRowContent()
		[
			SNew(SWrapBox)
			.UseAllottedWidth(true)
			+SWrapBox::Slot()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Open Module Editor")))
				.OnClicked_Raw(this, &FMainModuleDetailsPanel::OnClickOpenModuleEditorButton)
			]
		];
}

FReply FMainModuleDetailsPanel::OnClickOpenModuleEditorButton()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FName("ModuleEditor"));
	return FReply::Handled();
}
