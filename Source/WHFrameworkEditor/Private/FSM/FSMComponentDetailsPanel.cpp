// Copyright Epic Games, Inc. All Rights Reserved.

#include "FSM/FSMComponentDetailsPanel.h"

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

FFSMComponentDetailsPanel::FFSMComponentDetailsPanel() {}

TSharedRef<IDetailCustomization> FFSMComponentDetailsPanel::MakeInstance()
{
	return MakeShared<FFSMComponentDetailsPanel>();
}

void FFSMComponentDetailsPanel::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	SelectedObjectsList = DetailLayoutBuilder.GetSelectedObjects();

	IDetailCategoryBuilder& CameraModuleCategory = DetailLayoutBuilder.EditCategory(FName("CameraStats"));

	TSharedRef<SWrapBox> CameraModuleActionBox = SNew(SWrapBox).UseAllottedWidth(true);
	CameraModuleActionBox->AddSlot()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Copy Camera Params")))
		]
	];

	CameraModuleCategory.AddCustomRow(FText::GetEmpty())
		.ValueContent()
		[
			CameraModuleActionBox
		];
}
