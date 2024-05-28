// Fill out your copyright notice in the Description page of Project Settings.

#include "Slate/Editor/Panel/SEditorMaskPanel.h"

#include "SlateOptMacros.h"
#include "WHFrameworkSlateStyle.h"
#include "Slate/Editor/Base/SEditorWidgetBase.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SEditorMaskPanel::Construct(const FArguments& InArgs)
{
	ParentWidget = InArgs._ParentWidget;
	
	ChildSlot
	[
		SNew(SOverlay)

		+SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBorder)
			.BorderImage(FWHFrameworkSlateStyle::Get().GetBrush("Icons.Box"))
			.BorderBackgroundColor(InArgs._MaskColor)
			[
				SNew(SButton)
				.ButtonStyle(&FWHFrameworkSlateStyle::Get().GetWidgetStyle<FButtonStyle>("Buttons.NoBorder"))
				.OnClicked(this, &SEditorMaskPanel::OnMaskButtonClicked)
			]
		]

		+SOverlay::Slot()
		.HAlign(InArgs._ContentHAlign)
		.VAlign(InArgs._ContentVAlign)
		.Padding(InArgs._ContentPadding)
		[
			InArgs._Content.Widget->AsShared()
		]
	];
}

FReply SEditorMaskPanel::OnMaskButtonClicked()
{
	if(ParentWidget)
	{
		ParentWidget->Close();
	}
	else
	{
		SetVisibility(EVisibility::Collapsed);
	}
	return FReply::Handled();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
