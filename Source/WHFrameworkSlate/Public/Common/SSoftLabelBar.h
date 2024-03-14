// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_TwoParams(FOnSortLabelIndexAndAscendingChanged, int32, bool);

struct FSortLabel
{
public:
	FSortLabel()
	{
		Label = FText::GetEmpty();
		Fill = 1.f;
	}
	
	FSortLabel(const FText& Label, const float Fill)
		: Label(Label),
		  Fill(Fill)
	{
	}

public:
	FText Label;
	float Fill;
};

class SSoftLabelBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSoftLabelBar) {}

		SLATE_ARGUMENT(FMargin, Padding)

		SLATE_ARGUMENT(TArray<FSortLabel>, SortLabels)

		SLATE_ATTRIBUTE(int32, SortLabelIndex)

		SLATE_ATTRIBUTE(bool, SortLabelAscending)

		SLATE_EVENT(FOnSortLabelIndexAndAscendingChanged, OnSortLabelIndexAndAscendingChanged)

	SLATE_END_ARGS()

	SSoftLabelBar();

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
