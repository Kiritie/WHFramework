#include "Slate/Editor/Misc/SEditorSimpleTitleBar.h"

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
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(FMargin(8.f, 0.f, 0.f, 0.f))
		[
			SNew(STextBlock)
			.Font(InArgs._TitleFont)
			.ColorAndOpacity(InArgs._TitleColor)
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
				.ButtonStyle(&FWHFrameworkSlateStyle::Get().GetWidgetStyle<FButtonStyle>("Buttons.DefaultNoBorder.Error"))
				.OnClicked(this, &SEditorSimpleTitleBar::OnCloseButtonClicked)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.ContentPadding(FMargin(10.f))
				[
					SNew(SImage)
					.Image(FWHFrameworkSlateStyle::Get().GetBrush("Icons.Close_2"))
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
