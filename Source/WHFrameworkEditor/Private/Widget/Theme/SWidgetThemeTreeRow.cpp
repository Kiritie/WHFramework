#include "SWidgetThemeTreeRow.h"

#include "Widget/Theme/WidgetThemeEditorSettings.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SWidgetThemeTreeRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	const TSharedPtr<FThemeTreeItem> Item = InArgs._Item;
	RowStyle = FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");
	if (Item->bRoot)
	{
		RowStyle.SetEvenRowBackgroundBrush(FSlateRoundedBoxBrush(FStyleColors::Header, 4.f));
		RowStyle.SetOddRowBackgroundBrush(FSlateRoundedBoxBrush(FStyleColors::Header, 4.f));
	}
	STableRow::Construct(STableRow::FArguments()
		.Style(&RowStyle)
		.ShowWires(true)
		.Padding(FMargin(6.f, Item->bRoot ? 7.f : 4.f))
		.ToolTipText(FText::FromString(Item->Tag.ToString() + TEXT("\n") + Item->Tooltip))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 7.f, 0.f)
			[
				SNew(SBox).WidthOverride(16.f).HeightOverride(16.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush(Item->bRoot ? "Sequencer.KeyDiamond" : Item->bShared ? "GenericLink" : "ClassIcon.BlueprintCore"))
					.ColorAndOpacity(Item->bRoot ? FStyleColors::AccentBlue : FStyleColors::Foreground)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle(Item->bRoot ? "Bold" : "Regular", Item->bRoot ? 11 : 10))
				.Text_Lambda([Item] { return UWidgetThemeEditorSettings::GetStyleTagDisplayName(Item->Tag); })
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 2.f, 0.f)
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FStyleColors::ForegroundHover)
				.Text(Item->bRoot ? NSLOCTEXT("WidgetTheme", "RootBadge", "ROOT")
					: Item->bShared ? NSLOCTEXT("WidgetTheme", "SharedBadge", "SHARED") : FText::GetEmpty())
			]
		], InOwnerTableView);
}
