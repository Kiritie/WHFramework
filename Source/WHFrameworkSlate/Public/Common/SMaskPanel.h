// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

class SBasePanel;

class SMaskPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMaskPanel) {}

		SLATE_ARGUMENT(FSlateColor, MaskColor)
		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)
		SLATE_ARGUMENT(TSharedPtr<SBasePanel>, PanelWidget)
		SLATE_DEFAULT_SLOT(FArguments, Content)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

private:
	FReply OnMaskButtonClicked();

private:
	TSharedPtr<SBasePanel> PanelWidget;
};
