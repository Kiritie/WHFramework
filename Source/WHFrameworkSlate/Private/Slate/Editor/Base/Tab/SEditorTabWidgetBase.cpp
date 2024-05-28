#include "Slate/Editor/Base/Tab/SEditorTabWidgetBase.h"

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
		.Padding(InArgs._Padding)
		[
			SNew(SBorder)
			.BorderImage(FWHFrameworkSlateStyle::Get().GetBrush("Icons.Box"))
			.BorderBackgroundColor(InArgs._BackgroundColor)
			.Padding(FMargin(0.f))
			.Visibility(EVisibility::SelfHitTestInvisible)
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(InArgs._LabelHeight)
					.Visibility_Lambda([InArgs](){ return InArgs._ShowLabel ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
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
								.LabelColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f)))
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
					.Visibility_Lambda([InArgs](){ return InArgs._ShowSplitLine ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
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
