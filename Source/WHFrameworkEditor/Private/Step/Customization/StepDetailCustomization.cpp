// Copyright Epic Games, Inc. All Rights Reserved.

#include "Step/Customization/StepDetailCustomization.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "UObject/UnrealType.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "IDetailGroup.h"
#include "IDetailsView.h"

#include "ScopedTransaction.h"
#include "Step/Base/StepBase.h"
#include "Windows/WindowsPlatformApplicationMisc.h"

FStepDetailCustomization::FStepDetailCustomization()
{
	
}

TSharedRef<IDetailCustomization> FStepDetailCustomization::MakeInstance()
{
	return MakeShared<FStepDetailCustomization>();
}

void FStepDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	FDetailCustomizationBase::CustomizeDetails(DetailLayoutBuilder);
	
	IDetailCategoryBuilder& OperationTargetCategory = DetailLayoutBuilder.EditCategory(FName("Operation Target"));
	
	OperationTargetCategory.AddProperty(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStepBase, OperationTarget)));
	OperationTargetCategory.AddProperty(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStepBase, bTrackTarget)));
	OperationTargetCategory.AddProperty(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStepBase, TrackTargetMode)));

	IDetailGroup& CameraViewGroup = OperationTargetCategory.AddGroup(FName("Camera View"), FText::FromString(TEXT("Camera View")), false, false);
	CameraViewGroup.AddPropertyRow(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStepBase, CameraViewParams)));

	CameraViewGroup.AddWidgetRow()
		.WholeRowContent()
		[
			SNew(SWrapBox)
			.UseAllottedWidth(true)
			+SWrapBox::Slot()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Get Camera View")))
				.OnClicked_Raw(this, &FStepDetailCustomization::OnClickGetCameraViewButton)
			]
			+SWrapBox::Slot()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Paste Camera View")))
				.OnClicked_Raw(this, &FStepDetailCustomization::OnClickPasteCameraViewButton)
			]
		];
}

FReply FStepDetailCustomization::OnClickGetCameraViewButton()
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

FReply FStepDetailCustomization::OnClickPasteCameraViewButton()
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
