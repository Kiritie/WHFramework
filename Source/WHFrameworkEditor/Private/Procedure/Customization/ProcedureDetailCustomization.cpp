// Copyright Epic Games, Inc. All Rights Reserved.

#include "Procedure/Customization/ProcedureDetailCustomization.h"

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
#include "Procedure/Base/ProcedureBase.h"
#include "Windows/WindowsPlatformApplicationMisc.h"

FProcedureDetailCustomization::FProcedureDetailCustomization()
{
	
}

TSharedRef<IDetailCustomization> FProcedureDetailCustomization::MakeInstance()
{
	return MakeShared<FProcedureDetailCustomization>();
}

void FProcedureDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	FDetailCustomizationBase::CustomizeDetails(DetailLayoutBuilder);
	
	IDetailCategoryBuilder& OperationTargetCategory = DetailLayoutBuilder.EditCategory(FName("Operation Target"));
	
	OperationTargetCategory.AddProperty(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureBase, OperationTarget)));
	OperationTargetCategory.AddProperty(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureBase, bTrackTarget)));
	OperationTargetCategory.AddProperty(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureBase, TrackTargetMode)));

	IDetailGroup& CameraViewGroup = OperationTargetCategory.AddGroup(FName("Camera View"), FText::FromString(TEXT("Camera View")), false, false);
	CameraViewGroup.AddPropertyRow(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureBase, CameraViewParams)));

	CameraViewGroup.AddWidgetRow()
		.WholeRowContent()
		[
			SNew(SWrapBox)
			.UseAllottedWidth(true)
			+SWrapBox::Slot()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Get Camera View")))
				.OnClicked_Raw(this, &FProcedureDetailCustomization::OnClickGetCameraViewButton)
			]
			+SWrapBox::Slot()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Paste Camera View")))
				.OnClicked_Raw(this, &FProcedureDetailCustomization::OnClickPasteCameraViewButton)
			]
		];
}

FReply FProcedureDetailCustomization::OnClickGetCameraViewButton()
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

FReply FProcedureDetailCustomization::OnClickPasteCameraViewButton()
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
