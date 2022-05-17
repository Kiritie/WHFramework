// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerControllerDetailsPanel.h"

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
#include "Debug/DebugModuleTypes.h"
#include "Gameplay/WHPlayerController.h"
#include "Global/GlobalTypes.h"

FPlayerControllerDetailsPanel::FPlayerControllerDetailsPanel() {}

TSharedRef<IDetailCustomization> FPlayerControllerDetailsPanel::MakeInstance()
{
	return MakeShared<FPlayerControllerDetailsPanel>();
}

void FPlayerControllerDetailsPanel::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	SelectedObjectsList = DetailLayoutBuilder.GetSelectedObjects();

	IDetailCategoryBuilder& PlayerControllerCategory = DetailLayoutBuilder.EditCategory(FName("CameraStats"));

	TSharedRef<SWrapBox> PlayerControllerActionBox = SNew(SWrapBox).UseAllottedWidth(true);
	PlayerControllerActionBox->AddSlot()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Copy Camera Params")))
			.OnClicked(FOnClicked::CreateSP(this, &FPlayerControllerDetailsPanel::OnClickCopyCameraParamsButton))
		]
	];

	PlayerControllerCategory.AddCustomRow(FText::GetEmpty())
		.ValueContent()
		[
			PlayerControllerActionBox
		];
}

FReply FPlayerControllerDetailsPanel::OnClickCopyCameraParamsButton()
{
	if(SelectedObjectsList.IsValidIndex(0))
	{
		if(AWHPlayerController* PlayerController = Cast<AWHPlayerController>(SelectedObjectsList[0]))
		{
			FCameraParams CameraParams;
			CameraParams.CameraLocation = PlayerController->GetCurrentCameraLocation();
			CameraParams.CameraRotation = PlayerController->GetCurrentCameraRotation();
			CameraParams.CameraDistance = PlayerController->GetCurrentCameraDistance();

			GCopiedCameraData = CameraParams;
			
			WHLog(WH_Controller, Log, TEXT("Copied Camera Params: Location = %s , Rotation = %s , Distance = %f"), *CameraParams.CameraLocation.ToString(), *CameraParams.CameraRotation.ToString(), CameraParams.CameraDistance);
		}
	}
	return FReply::Handled();
}
