#pragma once
#include "Widgets/SCompoundWidget.h"
#include "Slate/Editor/Tab/SEditorTabListRow.h"

class SEditorTabWidgetBase;

DECLARE_DELEGATE_OneParam(FOnActivatedTabsChanged, const TArray<TSharedPtr<FEditorTabListItem>>&)

class WHFRAMEWORKSLATE_API SEditorTabList : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SEditorTabList)
		:_Style(&FWHFrameworkSlateStyle::Get().GetWidgetStyle<FTableRowStyle>("TableRows.Lists.Tab")),
		 _bKeepOneActivated(false),
		 _SelectionMode(ESelectionMode::Single)
	{}

		SLATE_STYLE_ARGUMENT(FTableRowStyle, Style)
		SLATE_ARGUMENT(FMargin, Padding)
		SLATE_ARGUMENT(bool, bKeepOneActivated)
		SLATE_ARGUMENT(ESelectionMode::Type, SelectionMode)
		SLATE_ARGUMENT(TArray<TSharedPtr<SEditorTabWidgetBase>>, TabWidgets)
		SLATE_EVENT(FOnActivatedTabsChanged, OnActivatedTabsChanged)

	SLATE_END_ARGS()

	SEditorTabList();
	
	void Construct(const FArguments& InArgs);

private:
	TSharedRef<ITableRow> GenerateListRow_Tabs(TSharedPtr<FEditorTabListItem> TabListItem, const TSharedRef<STableViewBase>& OwnerTable);

	void ListSelectionChanged_Tabs(TSharedPtr<FEditorTabListItem> TabListItem, ESelectInfo::Type SelectInfo);

private:
	const FTableRowStyle* Style;

	FMargin Padding;

	bool bKeepOneActivated;

	FOnActivatedTabsChanged OnActivatedTabsChanged;

	TSharedPtr<SListView<TSharedPtr<FEditorTabListItem>>> ListView_Tabs;

	TArray<TSharedPtr<SEditorTabWidgetBase>> TabWidgets;

	TArray<TSharedPtr<FEditorTabListItem>> TabListItems;
	TArray<TSharedPtr<FEditorTabListItem>> SelectedTabListItem;
	

public:
	int32 GetTabWidgetIndex() const;

	bool IsKeepOneActivated() const { return bKeepOneActivated; }

	TArray<TSharedPtr<FEditorTabListItem>> GetTabListItems() { return TabListItems; }

	TArray<TSharedPtr<FEditorTabListItem>> GetSelectedTabListItems() const { return SelectedTabListItem; }
};
