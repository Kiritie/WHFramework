#include "Slate/Editor/Setting/SEditorSettingCategoryBase.h"

#include "Slate/Editor/Misc/SEditorSplitLine.h"

SEditorSettingCategoryBase::SEditorSettingCategoryBase()
{
}

void SEditorSettingCategoryBase::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.f))
		.Padding(InArgs._Padding)
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(FMargin(0.f, 0.f, 0.f, 15.f))
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
				.Text(InArgs._Label)
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f))
			]

			+SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(FMargin(0.f, 0.f, 0.f, 10.f))
			.AutoHeight()
			[
				InArgs._Content.Widget != SNullWidget::NullWidget ? InArgs._Content.Widget : 
					SNew(SSpacer)
					.Visibility(EVisibility::Collapsed)
			]
		
			+SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.AutoHeight()
			[
				SNew(SEditorSplitLine)
				.Height(1.f)
				.Visibility_Lambda([InArgs](){ return InArgs._ShowSplitLine ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			]
		]
	];
}
