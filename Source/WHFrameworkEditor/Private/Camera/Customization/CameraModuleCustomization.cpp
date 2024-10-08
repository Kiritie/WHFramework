// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/Customization/CameraModuleCustomization.h"

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
#include "Camera/CameraModule.h"
#include "Debug/DebugModuleTypes.h"
#include "Windows/WindowsPlatformApplicationMisc.h"

FCameraModuleCustomization::FCameraModuleCustomization()
{
	
}

TSharedRef<IDetailCustomization> FCameraModuleCustomization::MakeInstance()
{
	return MakeShared<FCameraModuleCustomization>();
}

void FCameraModuleCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	FClassCustomizationBase::CustomizeDetails(DetailLayoutBuilder);

	IDetailCategoryBuilder& CameraModuleCategory = DetailLayoutBuilder.EditCategory(FName("CameraStats"));

	CameraModuleCategory.AddCustomRow(FText::GetEmpty())
		.WholeRowContent()
		[
			SNew(SWrapBox)
			.UseAllottedWidth(true)
			+SWrapBox::Slot()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Copy Camera Params")))
				.OnClicked(FOnClicked::CreateSP(this, &FCameraModuleCustomization::OnClickCopyCameraParamsButton))
				.IsEnabled_Lambda([](){ return GIsPlaying; })
			]
		];
}

FReply FCameraModuleCustomization::OnClickCopyCameraParamsButton()
{
	if(SelectedObjectsList.IsValidIndex(0))
	{
		if(UCameraModule* CameraModule = Cast<UCameraModule>(SelectedObjectsList[0]))
		{
			FCameraParams CameraParams;
			CameraParams.CameraLocation = CameraModule->GetCurrentCameraLocation();
			CameraParams.CameraRotation = CameraModule->GetCurrentCameraRotation();
			CameraParams.CameraDistance = CameraModule->GetCurrentCameraDistance();

			FPlatformApplicationMisc::ClipboardCopy(*CameraParams.ToString());
			
			WHLog(FString::Printf(TEXT("Copied Camera Params: Location = %s , Rotation = %s , Distance = %f"), *CameraParams.CameraLocation.ToString(), *CameraParams.CameraRotation.ToString(), CameraParams.CameraDistance), EDC_Camera);
		}
	}
	return FReply::Handled();
}
