// Copyright Epic Games, Inc. All Rights Reserved.

#include "Main/MainModuleDetailsPanel.h"

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
#include "Main/MainModule.h"

FMainModuleDetailsPanel::FMainModuleDetailsPanel() {}

TSharedRef<IDetailCustomization> FMainModuleDetailsPanel::MakeInstance()
{
	return MakeShared<FMainModuleDetailsPanel>();
}

void FMainModuleDetailsPanel::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	SelectedObjectsList = DetailLayoutBuilder.GetSelectedObjects();

	TSharedRef<IPropertyHandle> ModuleClassesProperty = DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(AMainModule, ModuleClasses));

	DetailLayoutBuilder.AddCustomRowToCategory(ModuleClassesProperty, FText::GetEmpty())
		.WholeRowContent()
		[
			SNew(SWrapBox)
			.UseAllottedWidth(true)
			+SWrapBox::Slot()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Generate Modules")))
				.OnClicked_Raw(this, &FMainModuleDetailsPanel::OnClickGenerateModulesButton)
			]
			+SWrapBox::Slot()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Destroy Modules")))
				.OnClicked_Raw(this, &FMainModuleDetailsPanel::OnClickDestroyModulesButton)
			]
		];
}

FReply FMainModuleDetailsPanel::OnClickGenerateModulesButton()
{
	if(SelectedObjectsList.IsValidIndex(0))
	{
		if(AMainModule* MainModule = Cast<AMainModule>(SelectedObjectsList[0]))
		{
			MainModule->GenerateModules();
		}
	}
	return FReply::Handled();
}

FReply FMainModuleDetailsPanel::OnClickDestroyModulesButton()
{
	if(SelectedObjectsList.IsValidIndex(0))
	{
		if(AMainModule* MainModule = Cast<AMainModule>(SelectedObjectsList[0]))
		{
			MainModule->DestroyModules();
		}
	}
	return FReply::Handled();
}
