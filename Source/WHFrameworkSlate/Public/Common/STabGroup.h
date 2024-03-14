// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_OneParam(FOnTabGroupActiveIndexChanged, int32);

class STabGroup : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STabGroup) {}

		SLATE_ARGUMENT(TArray<FText>, TabLabels)

		SLATE_ATTRIBUTE(int32, ActivedTabIndex)

		SLATE_EVENT(FOnTabGroupActiveIndexChanged, OnActiveIndexChanged)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

private:
	TSharedPtr<SHorizontalBox> HBox_Tabs;

	int32 ActivedTabIndex;

	FOnTabGroupActiveIndexChanged OnActiveIndexChanged;
};
