#include "Slate/Editor/Misc/SEditorSimpleWindow.h"

#include "WHFrameworkSlateStyle.h"
#include "Slate/Editor/Misc/SEditorSimpleTitleBar.h"
#include "Slate/Editor/Misc/SEditorSplitLine.h"

SEditorSimpleWindow::SEditorSimpleWindow()
{
	ParentWidget = nullptr;
}

void SEditorSimpleWindow::Construct(const FArguments& InArgs)
{
	ParentWidget = InArgs._ParentWidget;

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(InArgs._WindowWidth)
		.HeightOverride(InArgs._WindowHeight)
		[
			SNew(SBorder)
			.BorderImage(FWHFrameworkSlateStyle::Get().GetBrush("Icons.Border_Radius_16"))
			.BorderBackgroundColor(FLinearColor(0.015f, 0.015f, 0.015f))
			.Padding(FMargin(0.f))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.AutoHeight()
				[
					SNew(SEditorSimpleTitleBar)
					.Title(InArgs._Title)
					.TitleFont(InArgs._TitleFont)
					.TitleColor(InArgs._TitleColor)
					.ParentWidget(ParentWidget)
				]

				+SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.AutoHeight()
				[
					SNew(SEditorSplitLine)
					.Height(1.f)
					.Color(FLinearColor(0.03f, 0.03f, 0.03f))
				]

				+SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.Padding(InArgs._ContentPadding)
				.FillHeight(1.f)
				[
					InArgs._Content.Widget
				]
			]
		]
	];
}
