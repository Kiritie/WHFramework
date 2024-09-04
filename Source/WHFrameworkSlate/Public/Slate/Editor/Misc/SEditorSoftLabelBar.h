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
		bSortable = true;
	}
	
	FEditorSortLabel(const TAttribute<FText>& Label, const float Fill, bool bSortable = true)
		: Label(Label),
		  Fill(Fill),
		  bSortable(bSortable)
	{
	}

public:
	TAttribute<FText> Label;
	float Fill;
	bool bSortable;
};

class WHFRAMEWORKSLATE_API SEditorSoftLabelBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEditorSoftLabelBar)
		: _SortLabelIndex(0)
		, _SortLabelAscending(false)
	{}

		SLATE_ARGUMENT(FMargin, Padding)

		SLATE_ARGUMENT(TArray<FEditorSortLabel>, SortLabels)

		SLATE_ATTRIBUTE(int32, SortLabelIndex)

		SLATE_ATTRIBUTE(bool, SortLabelAscending)

		SLATE_EVENT(FOnSortLabelIndexAndAscendingChanged, OnSortLabelIndexAndAscendingChanged)

	SLATE_END_ARGS()

	SEditorSoftLabelBar();

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

public:
	float GetSortLabelFill(int32 InIndex) const;

private:
	TSharedPtr<SHorizontalBox> HBox_Labels;

	int32 SortLabelIndex;
	
	bool SortLabelAscending;

	TArray<FEditorSortLabel> SortLabels;

	FOnSortLabelIndexAndAscendingChanged OnSortLabelIndexAndAscendingChanged;

public:
	TArray<FEditorSortLabel> GetSortLabels() const { return SortLabels; }
	
	int32 GetSortLabelIndex() const { return SortLabelIndex; }

	bool IsSortLabelAscending() const { return SortLabelAscending; }
};
