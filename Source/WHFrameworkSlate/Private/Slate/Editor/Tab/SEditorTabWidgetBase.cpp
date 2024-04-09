#include "Slate/Editor/Tab/SEditorTabWidgetBase.h"

#include "WHFrameworkSlateStyle.h"
#include "Slate/Editor/Misc/SEditorSplitLine.h"
#include "Slate/Editor/Tab/SEditorTabLabel.h"

FName SEditorTabWidgetBase::WidgetName = FName("EditorTabWidgetBase");

SEditorTabWidgetBase::SEditorTabWidgetBase()
{
}

void SEditorTabWidgetBase::Construct(const FArguments& InArgs, const TSharedPtr<FEditorTabListItem>& InListItem)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());

	TabListItem = InListItem;

	ChildSlot
	[
		SNew(SOverlay)

		+SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(FMargin(20.f, 0.f))
		[
			SNew(SBorder)
			.BorderImage(FWHFrameworkSlateStyle::Get().GetBrush("Icons.White"))
			.BorderBackgroundColor(FLinearColor(0.01f, 0.01f, 0.01f))
			.Padding(FMargin(0.f))
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(InArgs._LabelHeight)
					[
						SNew(SHorizontalBox)

						+SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(SButton)
							.ButtonStyle(&FWHFrameworkSlateStyle::Get().GetWidgetStyle<FButtonStyle>("Buttons.NoBorder"))
							.OnClicked(InArgs._OnLabelClicked)
							[
								SNew(SEditorTabLabel)
								.Icon(&TabListItem->TabIcon->InactiveBrush)
								.Label(FText::FromString(TabListItem->TabName))
								.Color(FSlateColor(FLinearColor(1.f, 1.f, 1.f)))
							]
						]

						+SHorizontalBox::Slot()
						.HAlign(HAlign_Fill)
						.VAlign(VAlign_Fill)
						[
							InArgs._LabelContent.Widget
						]
					]
				]

				+SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.AutoHeight()
				[
					SNew(SEditorSplitLine)
					.Visibility_Lambda([InArgs](){ return InArgs._ShowSplitLine ? EVisibility::SelfHitTestInvisible : EVisibility::Hidden; })
					.Height(1.f)
				]

				+SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.FillHeight(1.f)
				[
					InArgs._Content.Widget
				]
			]
		]
	];
}

void SEditorTabWidgetBase::OnOpen(bool bInstant)
{
	SEditorWidgetBase::OnOpen(bInstant);
}

void SEditorTabWidgetBase::OnClose(bool bInstant)
{
	SEditorWidgetBase::OnClose(bInstant);
}

void SEditorTabWidgetBase::OnRefresh()
{
	SEditorWidgetBase::OnRefresh();
}
