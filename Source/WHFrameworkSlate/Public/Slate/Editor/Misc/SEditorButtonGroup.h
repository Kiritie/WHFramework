// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_TwoParams(FOnButtonGroupActiveIndexChanged, int32, ESelectInfo::Type);

struct WHFRAMEWORKSLATE_API FEditorGroupButtonItem
{
public:
	FEditorGroupButtonItem()
	{
		Label = FText::GetEmpty();
		Icon = nullptr;
	}

	FEditorGroupButtonItem(const FText& Label, const FSlateBrush* const Icon)
	: Label(Label),
	  Icon(Icon)
	{
	}

public:
	FText Label;

	const FSlateBrush* Icon;
};

class WHFRAMEWORKSLATE_API SEditorButtonGroup : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEditorButtonGroup)
		:_ButtonWidth(40.f),
		 _ButtonHeight(40.f),
		 _ButtonPadding(FMargin(10.f))
	{}

		SLATE_ARGUMENT(TArray<FEditorGroupButtonItem>, ButtonItems)

		SLATE_ATTRIBUTE(FOptionalSize, ButtonWidth)

		SLATE_ATTRIBUTE(FOptionalSize, ButtonHeight)

		SLATE_ARGUMENT(FMargin, ButtonPadding)

		SLATE_ARGUMENT(int32, DefaultActivedButtonIndex)

		SLATE_EVENT(FOnButtonGroupActiveIndexChanged, OnActiveButtonIndexChanged)

	SLATE_END_ARGS()

	SEditorButtonGroup();
	
	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

private:
	TSharedPtr<SHorizontalBox> HBox_Buttons;

	int32 ActivedButtonIndex;

	FOnButtonGroupActiveIndexChanged OnActiveButtonIndexChanged;

public:
	int32 GetActivedButtonIndex() const { return ActivedButtonIndex; }

	void SetActivedButtonIndex(int32 InIndex, ESelectInfo::Type InSelectInfo = ESelectInfo::Direct);
};
