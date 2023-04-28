// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/CameraModuleDetailsPanel.h"

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
#include "Camera/CameraModule.h"
#include "Debug/DebugModuleTypes.h"
#include "Global/GlobalTypes.h"
#include "Windows/WindowsPlatformApplicationMisc.h"

FCameraModuleDetailsPanel::FCameraModuleDetailsPanel() {}

TSharedRef<IDetailCustomization> FCameraModuleDetailsPanel::MakeInstance()
{
	return MakeShared<FCameraModuleDetailsPanel>();
}

void FCameraModuleDetailsPanel::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
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
			.OnClicked(FOnClicked::CreateSP(this, &FCameraModuleDetailsPanel::OnClickCopyCameraParamsButton))
		]
	];

	CameraModuleCategory.AddCustomRow(FText::GetEmpty())
		.ValueContent()
		[
			CameraModuleActionBox
		];
}

FReply FCameraModuleDetailsPanel::OnClickCopyCameraParamsButton()
{
	if(SelectedObjectsList.IsValidIndex(0))
	{
		if(ACameraModule* CameraModule = Cast<ACameraModule>(SelectedObjectsList[0]))
		{
			FCameraParams CameraParams;
			CameraParams.CameraLocation = CameraModule->GetCurrentCameraLocation();
			CameraParams.CameraRotation = CameraModule->GetCurrentCameraRotation();
			CameraParams.CameraDistance = CameraModule->GetCurrentCameraDistance();

			FPlatformApplicationMisc::ClipboardCopy(*CameraParams.ToString());
			
			WHLog(FString::Printf(TEXT("Copied Camera Params: Location = %s , Rotation = %s , Distance = %f"), *CameraParams.CameraLocation.ToString(), *CameraParams.CameraRotation.ToString(), CameraParams.CameraDistance), EDebugCategory::Camera);
		}
	}
	return FReply::Handled();
}
