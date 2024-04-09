// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

class SEditorWidgetBase;

class WHFRAMEWORKSLATE_API SEditorMaskPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEditorMaskPanel) {}

		SLATE_ARGUMENT(FSlateColor, MaskColor)
		SLATE_ARGUMENT(EHorizontalAlignment, ContentHAlign)
		SLATE_ARGUMENT(EVerticalAlignment, ContentVAlign)
		SLATE_ARGUMENT(FMargin, ContentPadding)
		SLATE_ARGUMENT(TSharedPtr<SEditorWidgetBase>, ParentWidget)
		SLATE_DEFAULT_SLOT(FArguments, Content)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

private:
	FReply OnMaskButtonClicked();

private:
	TSharedPtr<SEditorWidgetBase> ParentWidget;
};
