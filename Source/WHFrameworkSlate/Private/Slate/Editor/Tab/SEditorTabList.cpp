#include "Slate/Editor/Tab/SEditorTabList.h"

#include "Slate/Editor/Base/Tab/SEditorTabWidgetBase.h"

SEditorTabList::SEditorTabList(): bKeepOneActivated(false)
{
	Style = nullptr;
}

void SEditorTabList::Construct(const FArguments& InArgs)
{
	Style = InArgs._Style;
	Padding = InArgs._Padding;
	bKeepOneActivated = InArgs._bKeepOneActivated;

	OnActivatedTabsChanged = InArgs._OnActivatedTabsChanged;
	
	TabWidgets = InArgs._TabWidgets;
	for(auto Iter : TabWidgets)
	{
		TabListItems.Add(Iter->GetTabListItem());
	}

	SAssignNew(ListView_Tabs, SListView<TSharedPtr<FEditorTabListItem>>)
	.Orientation(Orient_Horizontal)
	.ListItemsSource(&TabListItems)
	.ItemHeight(18)
	.SelectionMode(InArgs._SelectionMode)
	.ClearSelectionOnClick(false)
	.OnGenerateRow(this, &SEditorTabList::GenerateListRow_Tabs)
	.OnSelectionChanged(this, &SEditorTabList::ListSelectionChanged_Tabs);

	for(auto Iter : TabListItems)
	{
		ListView_Tabs->SetItemSelection(Iter, Iter->bActivated);
	}

	ChildSlot
	[
		ListView_Tabs->AsShared()
	];
}

TSharedRef<ITableRow> SEditorTabList::GenerateListRow_Tabs(TSharedPtr<FEditorTabListItem> TabListItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SEditorTabListRow, OwnerTable)
		.Style(Style)
		.Padding(Padding)
		.bKeepOneActivated(bKeepOneActivated)
		.TabListItem(TabListItem);
}

void SEditorTabList::ListSelectionChanged_Tabs(TSharedPtr<FEditorTabListItem> TabListItem, ESelectInfo::Type SelectInfo)
{
	SelectedTabListItem = ListView_Tabs->GetSelectedItems();

	if(OnActivatedTabsChanged.IsBound())
	{
		OnActivatedTabsChanged.Execute(SelectedTabListItem);
	}
	
	for(auto Iter : TabWidgets)
	{
		if(SelectedTabListItem.Contains(Iter->GetTabListItem()))
		{
			Iter->Open();
		}
		else
		{
			Iter->Close();
		}
	}
}

int32 SEditorTabList::GetTabWidgetIndex() const
{
	return TabListItems.Find(SelectedTabListItem[0]);
}
