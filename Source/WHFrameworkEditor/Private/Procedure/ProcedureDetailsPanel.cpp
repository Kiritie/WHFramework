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
#include "Windows/WindowsPlatformApplicationMisc.h"

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
			.OnClicked(FOnClicked::CreateSP(this, &FProcedureDetailsPanel::OnClickGetCameraViewButton))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Paste Camera View")))
			.OnClicked(FOnClicked::CreateSP(this, &FProcedureDetailsPanel::OnClickPasteCameraViewButton))
		]
	];

	OperationTargetCategory.AddCustomRow(FText::GetEmpty())
		.ValueContent()
		[
			CameraViewActionBox
		];
}

FReply FProcedureDetailsPanel::OnClickGetCameraViewButton()
{
	for(const TWeakObjectPtr<UObject>& SelectedObject : SelectedObjectsList)
	{
		if(UProcedureBase* Procedure = Cast<UProcedureBase>(SelectedObject.Get()))
		{
			Procedure->GetCameraView();
		}
	}

	return FReply::Handled();
}

FReply FProcedureDetailsPanel::OnClickPasteCameraViewButton()
{
	for(const TWeakObjectPtr<UObject>& SelectedObject : SelectedObjectsList)
	{
		if(UProcedureBase* Procedure = Cast<UProcedureBase>(SelectedObject.Get()))
		{
			FString CameraParams;
			FPlatformApplicationMisc::ClipboardPaste(CameraParams);
			Procedure->SetCameraView(FCameraParams(CameraParams));
		}
	}

	return FReply::Handled();
}
