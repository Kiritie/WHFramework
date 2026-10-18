#pragma once

#include "Widget/Theme/WidgetThemeTree.h"
#include "Widgets/Views/STableRow.h"

class SWidgetThemeTreeRow : public STableRow<TSharedPtr<FThemeTreeItem>>
{
public:
	SLATE_BEGIN_ARGS(SWidgetThemeTreeRow) {}
	SLATE_ARGUMENT(TSharedPtr<FThemeTreeItem>, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

private:
	FTableRowStyle RowStyle;
};
