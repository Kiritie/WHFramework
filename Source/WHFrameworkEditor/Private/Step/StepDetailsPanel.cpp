// Copyright Epic Games, Inc. All Rights Reserved.

#include "Step/StepDetailsPanel.h"

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
#include "Step/Base/StepBase.h"
#include "Windows/WindowsPlatformApplicationMisc.h"

FStepDetailsPanel::FStepDetailsPanel() {}

TSharedRef<IDetailCustomization> FStepDetailsPanel::MakeInstance() { return MakeShared<FStepDetailsPanel>(); }

void FStepDetailsPanel::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	SelectedObjectsList = DetailLayoutBuilder.GetSelectedObjects();

	IDetailCategoryBuilder& OperationTargetCategory = DetailLayoutBuilder.EditCategory(FName("Operation Target"));

	OperationTargetCategory.AddProperty(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStepBase, OperationTarget)));
	OperationTargetCategory.AddProperty(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStepBase, bTrackTarget)));

	TSharedRef<SWrapBox> CameraViewActionBox = SNew(SWrapBox).UseAllottedWidth(true);
	CameraViewActionBox->AddSlot()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Get Camera View")))
			.OnClicked(FOnClicked::CreateSP(this, &FStepDetailsPanel::OnClickGetCameraViewButton))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Paste Camera View")))
			.OnClicked(FOnClicked::CreateSP(this, &FStepDetailsPanel::OnClickPasteCameraViewButton))
		]
	];

	OperationTargetCategory.AddCustomRow(FText::GetEmpty())
		.ValueContent()
		[
			CameraViewActionBox
		];
}

FReply FStepDetailsPanel::OnClickGetCameraViewButton()
{
	for(const TWeakObjectPtr<UObject>& SelectedObject : SelectedObjectsList)
	{
		if(UStepBase* Step = Cast<UStepBase>(SelectedObject.Get()))
		{
			Step->GetCameraView();
		}
	}

	return FReply::Handled();
}

FReply FStepDetailsPanel::OnClickPasteCameraViewButton()
{
	for(const TWeakObjectPtr<UObject>& SelectedObject : SelectedObjectsList)
	{
		if(UStepBase* Step = Cast<UStepBase>(SelectedObject.Get()))
		{
			FString CameraParams;
			FPlatformApplicationMisc::ClipboardPaste(CameraParams);
			Step->SetCameraView(FCameraParams(CameraParams));
		}
	}

	return FReply::Handled();
}
