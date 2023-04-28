// Copyright Epic Games, Inc. All Rights Reserved.

#include "Procedure/ProcedureDetailsPanel.h"

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
	OperationTargetCategory.AddProperty(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureBase, TrackTargetMode)));

	TSharedRef<IPropertyHandle> ModuleClassesProperty = DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureBase, CameraViewPawn));

	IDetailGroup& CameraViewGroup = OperationTargetCategory.AddGroup(FName("Camera View"), FText::FromString(TEXT("Camera View")), false, false);
	CameraViewGroup.AddPropertyRow(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureBase, CameraViewPawn)));
	CameraViewGroup.AddPropertyRow(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureBase, CameraViewMode)));
	CameraViewGroup.AddPropertyRow(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureBase, CameraViewSpace)));
	CameraViewGroup.AddPropertyRow(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureBase, CameraViewEaseType)));
	CameraViewGroup.AddPropertyRow(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureBase, CameraViewDuration)));
	CameraViewGroup.AddPropertyRow(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureBase, CameraViewOffset)));
	CameraViewGroup.AddPropertyRow(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureBase, CameraViewYaw)));
	CameraViewGroup.AddPropertyRow(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureBase, CameraViewPitch)));
	CameraViewGroup.AddPropertyRow(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureBase, CameraViewDistance)));

	CameraViewGroup.AddWidgetRow()
		.WholeRowContent()
		[
			SNew(SWrapBox)
			.UseAllottedWidth(true)
			+SWrapBox::Slot()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Get Camera View")))
				.OnClicked_Raw(this, &FProcedureDetailsPanel::OnClickGetCameraViewButton)
			]
			+SWrapBox::Slot()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Paste Camera View")))
				.OnClicked_Raw(this, &FProcedureDetailsPanel::OnClickPasteCameraViewButton)
			]
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
