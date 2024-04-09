// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_TwoParams(FOnSortLabelIndexAndAscendingChanged, int32, bool);

struct WHFRAMEWORKSLATE_API FEditorSortLabel
{
public:
	FEditorSortLabel()
	{
		Label = FText::GetEmpty();
		Fill = 1.f;
	}
	
	FEditorSortLabel(const FText& Label, const float Fill)
		: Label(Label),
		  Fill(Fill)
	{
	}

public:
	FText Label;
	float Fill;
};

class WHFRAMEWORKSLATE_API SEditorSoftLabelBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEditorSoftLabelBar) {}

		SLATE_ARGUMENT(FMargin, Padding)

		SLATE_ARGUMENT(TArray<FEditorSortLabel>, SortLabels)

		SLATE_ATTRIBUTE(int32, SortLabelIndex)

		SLATE_ATTRIBUTE(bool, SortLabelAscending)

		SLATE_EVENT(FOnSortLabelIndexAndAscendingChanged, OnSortLabelIndexAndAscendingChanged)

	SLATE_END_ARGS()

	SEditorSoftLabelBar();

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

private:
	TSharedPtr<SHorizontalBox> HBox_Labels;

	int32 SortLabelIndex;
	
	bool SortLabelAscending;

	FOnSortLabelIndexAndAscendingChanged OnSortLabelIndexAndAscendingChanged;

public:
	int32 GetSortLabelIndex() const { return SortLabelIndex; }

	bool IsSortLabelAscending() const { return SortLabelAscending; }
};
