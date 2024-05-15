// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/Customization/CameraPointCustomization.h"

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
#include "Camera/Point/CameraPointBase.h"
#include "Windows/WindowsPlatformApplicationMisc.h"

FCameraPointCustomization::FCameraPointCustomization()
{
	
}

TSharedRef<IDetailCustomization> FCameraPointCustomization::MakeInstance()
{
	return MakeShared<FCameraPointCustomization>();
}

void FCameraPointCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	FClassCustomizationBase::CustomizeDetails(DetailLayoutBuilder);
	
	TSharedRef<IPropertyHandle> CameraViewParamsProperty = DetailLayoutBuilder.GetProperty(FName("CameraViewParams"));

	DetailLayoutBuilder.AddCustomRowToCategory(CameraViewParamsProperty, FText::GetEmpty())
		.WholeRowContent()
		[
			SNew(SWrapBox)
			.UseAllottedWidth(true)
			+SWrapBox::Slot()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Get Camera View")))
				.OnClicked_Raw(this, &FCameraPointCustomization::OnClickGetCameraViewButton)
			]
			+SWrapBox::Slot()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Paste Camera View")))
				.OnClicked_Raw(this, &FCameraPointCustomization::OnClickPasteCameraViewButton)
			]
			+SWrapBox::Slot()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Switch Camera Point")))
				.OnClicked_Raw(this, &FCameraPointCustomization::OnClickSwitchCameraPointButton)
			]
		];
}

FReply FCameraPointCustomization::OnClickGetCameraViewButton()
{
	if(GIsPlaying)
	{
		for(const TWeakObjectPtr<UObject>& SelectedObject : SelectedObjectsList)
		{
			if(ACameraPointBase* CameraPoint = Cast<ACameraPointBase>(SelectedObject.Get()))
			{
				CameraPoint->GetCameraView();
			}
		}
	}

	return FReply::Handled();
}

FReply FCameraPointCustomization::OnClickPasteCameraViewButton()
{
	if(GIsPlaying)
	{
		for(const TWeakObjectPtr<UObject>& SelectedObject : SelectedObjectsList)
		{
			if(ACameraPointBase* CameraPoint = Cast<ACameraPointBase>(SelectedObject.Get()))
			{
				FString CameraParams;
				FPlatformApplicationMisc::ClipboardPaste(CameraParams);
				CameraPoint->SetCameraView(FCameraParams(CameraParams));
			}
		}
	}

	return FReply::Handled();
}

FReply FCameraPointCustomization::OnClickSwitchCameraPointButton()
{
	if(GIsPlaying)
	{
		for(const TWeakObjectPtr<UObject>& SelectedObject : SelectedObjectsList)
		{
			if(ACameraPointBase* CameraPoint = Cast<ACameraPointBase>(SelectedObject.Get()))
			{
				CameraPoint->Switch();
			}
		}
	}

	return FReply::Handled();
}
