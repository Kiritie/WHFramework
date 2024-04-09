// Fill out your copyright notice in the Description page of Project Settings.

#include "Slate/Editor/Misc/SEditorButtonGroup.h"

#include "SlateOptMacros.h"
#include "WHFrameworkSlateStyle.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SEditorButtonGroup::SEditorButtonGroup(): ActivedButtonIndex(0)
{
}

void SEditorButtonGroup::Construct(const FArguments& InArgs)
{
	ActivedButtonIndex = InArgs._DefaultActivedButtonIndex;
	
	OnActiveButtonIndexChanged = InArgs._OnActiveButtonIndexChanged;
	
	SAssignNew(HBox_Buttons, SHorizontalBox);

	for(int32 i = 0; i < InArgs._ButtonItems.Num(); i++)
	{
		HBox_Buttons->AddSlot()
		.HAlign(HAlign_Left)
		.AutoWidth()
		.Padding(InArgs._ButtonPadding)
		[
			SNew(SBox)
			.WidthOverride(InArgs._ButtonWidth)
			.HeightOverride(InArgs._ButtonHeight)
			[
				SNew(SCheckBox)
				.Style(&FWHFrameworkSlateStyle::Get().GetWidgetStyle<FCheckBoxStyle>("CheckBoxes.DefaultNoBorder"))
				.Padding(FMargin(0.f))
				.IsChecked_Lambda([this, i]()
				{
					return ActivedButtonIndex == i ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this, i](ECheckBoxState State)
				{
					if(State == ECheckBoxState::Checked)
					{
						SetActivedButtonIndex(i, ESelectInfo::OnMouseClick);
					}
				})
				[
					SNew(SBox)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(InArgs._ButtonItems[i].Icon)
					]
				]
			]
		];
	}

	ChildSlot
	[
		HBox_Buttons->AsShared()
	];
}

void SEditorButtonGroup::SetActivedButtonIndex(int32 InIndex, ESelectInfo::Type InSelectInfo)
{
	ActivedButtonIndex = InIndex;
	if(OnActiveButtonIndexChanged.IsBound())
	{
		OnActiveButtonIndexChanged.Execute(ActivedButtonIndex, InSelectInfo);
	}
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
