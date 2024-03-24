// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

struct WHFRAMEWORKSLATE_API FTabListItem : public TSharedFromThis<FTabListItem>
{
public:
	FTabListItem()
	{
		TabName = TEXT("");
		TabIcon = nullptr;
	}

	FTabListItem(const FString& TabName, const FSlateBrush* TabIcon)
		: TabName(TabName),
		  TabIcon(TabIcon)
	{
	}

public:
	FString TabName;
	const FSlateBrush* TabIcon;
};

class WHFRAMEWORKSLATE_API STabListItem : public STableRow<TSharedPtr<FTabListItem>>
{
public:
	SLATE_BEGIN_ARGS(STabListItem) {}

		SLATE_ARGUMENT(FMargin, Padding)
		SLATE_ARGUMENT(TSharedPtr<FTabListItem>, TabListItem)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

private:
	TSharedPtr<FTabListItem> TabListItem;
};
