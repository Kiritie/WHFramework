﻿#include "Slate/Editor/Misc/SEditorSimpleTitleBar.h"

#include "WHFrameworkSlateStyle.h"

SEditorSimpleTitleBar::SEditorSimpleTitleBar()
{
	ParentWidget = nullptr;
}

void SEditorSimpleTitleBar::Construct(const FArguments& InArgs)
{
	ParentWidget = InArgs._ParentWidget;
	
	ChildSlot
	[
		SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Fill)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 20))
			.ColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f)))
			.Text(InArgs._Title)
		]

		+SHorizontalBox::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.FillWidth(1.f)
		[
			SNew(SSpacer)
		]

		+SHorizontalBox::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(30.f)
			.HeightOverride(30.f)
			[
				SNew(SButton)
				.ButtonStyle(&FWHFrameworkSlateStyle::Get().GetWidgetStyle<FButtonStyle>("Buttons.DefaultNoBorder"))
				.OnClicked(this, &SEditorSimpleTitleBar::OnCloseButtonClicked)
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(FWHFrameworkSlateStyle::Get().GetBrush("Icons.Close"))
					]
				]
			]
		]
	];
}

FReply SEditorSimpleTitleBar::OnCloseButtonClicked()
{
	if(ParentWidget)
	{
		ParentWidget->Close();
	}
	return FReply::Handled();
}
