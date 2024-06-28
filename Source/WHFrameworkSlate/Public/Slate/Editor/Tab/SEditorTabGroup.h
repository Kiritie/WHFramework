// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_TwoParams(FOnTabGroupActiveIndexChanged, int32, ESelectInfo::Type);

struct WHFRAMEWORKSLATE_API FEditorTabLabel
{
public:
	FEditorTabLabel()
	{
		Label = FText::GetEmpty();
		Icon = nullptr;
	}
	
	FEditorTabLabel(const FText& Label, const FSlateBrush* Icon = nullptr)
		: Label(Label),
		  Icon(Icon)
	{
	}

public:
	FText Label;
	const FSlateBrush* Icon;
};

class WHFRAMEWORKSLATE_API SEditorTabGroup : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEditorTabGroup)
		: _LabelFont(FCoreStyle::GetDefaultFontStyle("Regular", 12))
		, _LabelColor(FSlateColor::UseForeground())
		, _LabelPadding(FMargin(10.f, 0.f, 10.f, 15.f))
	{}

		SLATE_ARGUMENT(TArray<FEditorTabLabel>, TabLabels)

		SLATE_ATTRIBUTE(FSlateFontInfo, LabelFont)

		SLATE_ATTRIBUTE(FSlateColor, LabelColor)

		SLATE_ATTRIBUTE(FMargin, LabelPadding)

		SLATE_ARGUMENT(int32, DefaultActivedTabIndex)

		SLATE_EVENT(FOnTabGroupActiveIndexChanged, OnActiveTabIndexChanged)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
	
	SEditorTabGroup();

public:
	void SetActivedTabIndex(int32 InIndex, ESelectInfo::Type InSelectInfo = ESelectInfo::Type::Direct);

private:
	TSharedPtr<SHorizontalBox> HBox_Tabs;

	int32 ActivedTabIndex;

	FOnTabGroupActiveIndexChanged OnActiveTabIndexChanged;
};
