// Fill out your copyright notice in the Description page of Project Settings.

#include "Slate/Editor/Tab/SEditorTabGroup.h"

#include "SlateOptMacros.h"
#include "WHFrameworkSlateStyle.h"
#include "Slate/Editor/Tab/SEditorTabLabel_UnderLine.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SEditorTabGroup::SEditorTabGroup()
{
	ActivedTabIndex = 0;
}

void SEditorTabGroup::Construct(const FArguments& InArgs)
{
	ActivedTabIndex = InArgs._DefaultActivedTabIndex;
	
	OnActiveTabIndexChanged = InArgs._OnActiveTabIndexChanged;
	
	SAssignNew(HBox_Tabs, SHorizontalBox);

	for(int32 i = 0; i < InArgs._TabLabels.Num(); i++)
	{
		HBox_Tabs->AddSlot()
		.HAlign(HAlign_Left)
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Style(&FWHFrameworkSlateStyle::Get().GetWidgetStyle<FCheckBoxStyle>("CheckBoxes.Tab.Default"))
			.Padding(FMargin(0.f))
			.IsChecked_Lambda([this, i]()
			{
				return ActivedTabIndex == i ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([this, i](ECheckBoxState State)
			{
				if(State == ECheckBoxState::Checked)
				{
					SetActivedTabIndex(i, ESelectInfo::OnMouseClick);
				}
			})
			[
				SNew(SEditorTabLabel_UnderLine)
				.Icon(InArgs._TabLabels[i].Icon)
				.Label(InArgs._TabLabels[i].Label)
				.LabelFont(InArgs._LabelFont)
				.LabelColor(InArgs._LabelColor)
				.LabelPadding(InArgs._LabelPadding)
				.bActived_Lambda([this, i](){ return ActivedTabIndex == i; })
			]
		];
	}

	ChildSlot
	[
		HBox_Tabs->AsShared()
	];
}

void SEditorTabGroup::SetActivedTabIndex(int32 InIndex, ESelectInfo::Type InSelectInfo)
{
	ActivedTabIndex = InIndex;
	if(OnActiveTabIndexChanged.IsBound())
	{
		OnActiveTabIndexChanged.Execute(ActivedTabIndex, InSelectInfo);
	}
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
