// Fill out your copyright notice in the Description page of Project Settings.

#include "Common/STabGroup.h"

#include "SlateOptMacros.h"
#include "WHFrameworkSlateStyle.h"
#include "Common/STabLabel.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void STabGroup::Construct(const FArguments& InArgs)
{
	ActivedTabIndex = InArgs._ActivedTabIndex.Get();
	
	OnActiveIndexChanged = InArgs._OnActiveIndexChanged;
	
	SAssignNew(HBox_Tabs, SHorizontalBox);

	for(int32 i = 0; i < InArgs._TabLabels.Num(); i++)
	{
		HBox_Tabs->AddSlot()
		.HAlign(HAlign_Left)
		.AutoWidth()
		.Padding(FMargin(10.f))
		[
			SNew(SButton)
			.ButtonStyle(&FWHFrameworkSlateStyle::Get().GetWidgetStyle<FButtonStyle>("Main.Button.Tab"))
			.ContentPadding(FMargin(0.f, 10.f))
			.OnClicked_Lambda([this, i]()
			{
				ActivedTabIndex = i;
				if(OnActiveIndexChanged.IsBound())
				{
					OnActiveIndexChanged.Execute(ActivedTabIndex);
				}
				return FReply::Handled();
			})
			[
				SNew(STabLabel)
				.Label(InArgs._TabLabels[i])
				.bActived_Lambda([this, i](){ return ActivedTabIndex == i; })
			]
		];
	}

	ChildSlot
	[
		HBox_Tabs->AsShared()
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
