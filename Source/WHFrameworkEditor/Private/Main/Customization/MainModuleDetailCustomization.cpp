// Copyright Epic Games, Inc. All Rights Reserved.

#include "Main/Customization/MainModuleDetailCustomization.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "UObject/UnrealType.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailsView.h"

#include "ScopedTransaction.h"
#include "Main/MainModule.h"
#include "Widgets/Layout/SWrapBox.h"

FMainModuleDetailCustomization::FMainModuleDetailCustomization()
{
	
}

TSharedRef<IDetailCustomization> FMainModuleDetailCustomization::MakeInstance()
{
	return MakeShared<FMainModuleDetailCustomization>();
}

void FMainModuleDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	FDetailCustomizationBase::CustomizeDetails(DetailLayoutBuilder);

	TSharedRef<IPropertyHandle> ModuleClassesProperty = DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(AMainModule, Modules));

	DetailLayoutBuilder.AddCustomRowToCategory(ModuleClassesProperty, FText::GetEmpty())
		.WholeRowContent()
		[
			SNew(SWrapBox)
			.UseAllottedWidth(true)
			+SWrapBox::Slot()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Open Module Editor")))
				.OnClicked_Raw(this, &FMainModuleDetailCustomization::OnClickOpenModuleEditorButton)
			]
		];
}

FReply FMainModuleDetailCustomization::OnClickOpenModuleEditorButton()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FName("ModuleEditor"));
	return FReply::Handled();
}
