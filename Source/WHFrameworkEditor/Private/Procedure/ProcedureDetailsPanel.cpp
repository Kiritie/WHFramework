// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProcedureDetailsPanel.h"

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
#include "Procedure/Base/ProcedureBase.h"

FProcedureDetailsPanel::FProcedureDetailsPanel() {}

TSharedRef<IDetailCustomization> FProcedureDetailsPanel::MakeInstance() { return MakeShared<FProcedureDetailsPanel>(); }

void FProcedureDetailsPanel::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	SelectedObjectsList = DetailLayoutBuilder.GetSelectedObjects();

	IDetailCategoryBuilder& OperationTargetCategory = DetailLayoutBuilder.EditCategory(FName("Operation Target"));

	OperationTargetCategory.AddProperty(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureBase, OperationTarget)));
	OperationTargetCategory.AddProperty(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureBase, bTrackTarget)));

	TSharedRef<SWrapBox> CameraViewActionBox = SNew(SWrapBox).UseAllottedWidth(true);
	CameraViewActionBox->AddSlot()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Get Camera View")))
			.OnClicked(FOnClicked::CreateSP(this, &FProcedureDetailsPanel::OnExecuteAction))
		]
	];

	OperationTargetCategory.AddCustomRow(FText::GetEmpty())
		.ValueContent()
		[
			CameraViewActionBox
		];
}

FReply FProcedureDetailsPanel::OnExecuteAction()
{
	for(const TWeakObjectPtr<UObject>& SelectedObject : SelectedObjectsList) { if(UProcedureBase* Procedure = Cast<UProcedureBase>(SelectedObject.Get())) { Procedure->GetCameraView(); } }

	return FReply::Handled();
}
