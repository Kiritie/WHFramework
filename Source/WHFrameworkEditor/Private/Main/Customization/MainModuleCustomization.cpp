// Copyright Epic Games, Inc. All Rights Reserved.

#include "Main/Customization/MainModuleCustomization.h"

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

FMainModuleCustomization::FMainModuleCustomization()
{
	
}

TSharedRef<IDetailCustomization> FMainModuleCustomization::MakeInstance()
{
	return MakeShared<FMainModuleCustomization>();
}

void FMainModuleCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	FClassCustomizationBase::CustomizeDetails(DetailLayoutBuilder);

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
				.OnClicked_Raw(this, &FMainModuleCustomization::OnClickOpenModuleEditorButton)
			]
		];
}

FReply FMainModuleCustomization::OnClickOpenModuleEditorButton()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FName("ModuleEditor"));
	return FReply::Handled();
}
