// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WHFrameworkSlateStyle.h"
#include "WHFrameworkSlateTypes.h"

#include "Widgets/SCompoundWidget.h"

struct WHFRAMEWORKSLATE_API FEditorTabListItem : public TSharedFromThis<FEditorTabListItem>
{
public:
	FEditorTabListItem()
	{
		TabName = TEXT("");
		TabIcon = nullptr;
		bActivated = false;
	}

	FEditorTabListItem(const FString& TabName, const FActivableIconStyle* TabIcon, bool bActivated = false)
		: TabName(TabName),
		  TabIcon(TabIcon),
		  bActivated(bActivated)
	{
	}

public:
	FString TabName;
	const FActivableIconStyle* TabIcon;
	bool bActivated;
};

class WHFRAMEWORKSLATE_API SEditorTabListRow : public STableRow<TSharedPtr<FEditorTabListItem>>
{
public:
	SLATE_BEGIN_ARGS(SEditorTabListRow)
		:_Style(&FWHFrameworkSlateStyle::Get().GetWidgetStyle<FTableRowStyle>("TableRows.List.Tab")),
		 _bHasUnderLine(false),
		 _bKeepOneActivated(false)
	{}

		SLATE_STYLE_ARGUMENT(FTableRowStyle, Style)
		SLATE_ARGUMENT(FMargin, Padding)
		SLATE_ARGUMENT(bool, bHasUnderLine)
		SLATE_ARGUMENT(bool, bKeepOneActivated)
		SLATE_ARGUMENT(TSharedPtr<FEditorTabListItem>, TabListItem)

	SLATE_END_ARGS()

	SEditorTabListRow();

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

protected:
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

private:
	TSharedPtr<FEditorTabListItem> TabListItem;

	bool bKeepOneActivated;
};
