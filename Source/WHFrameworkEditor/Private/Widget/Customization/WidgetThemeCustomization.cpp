// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Customization/WidgetThemeCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widget/Theme/WidgetTheme.h"

#define LOCTEXT_NAMESPACE "WidgetThemeCustomization"

TSharedRef<IDetailCustomization> FWidgetThemeCustomization::MakeInstance()
{
	return MakeShared<FWidgetThemeCustomization>();
}

void FWidgetThemeCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	FClassCustomizationBase::CustomizeDetails(DetailLayoutBuilder);
	DetailBuilder = &DetailLayoutBuilder;

	IDetailCategoryBuilder& StyleCategory = DetailLayoutBuilder.EditCategory(
		TEXT("Style"),
		LOCTEXT("StyleCategory", "Style"),
		ECategoryPriority::Important);
	StyleCategory.AddCustomRow(LOCTEXT("SortStylesFilter", "Sort Styles"))
	.WholeRowContent()
	[
		SNew(SButton)
		.Text(LOCTEXT("SortStyles", "Sort Styles by Tag"))
		.ToolTipText(LOCTEXT("SortStylesTooltip", "Sort button, text, and brush styles by gameplay tag."))
		.OnClicked(this, &FWidgetThemeCustomization::OnSortStyles)
	];
}

FReply FWidgetThemeCustomization::OnSortStyles()
{
	TArray<UWidgetTheme*> Themes;
	for(const TWeakObjectPtr<UObject>& SelectedObject : SelectedObjectsList)
	{
		if(UWidgetTheme* Theme = Cast<UWidgetTheme>(SelectedObject.Get());
			Theme && !Theme->AreStylesSorted())
		{
			Themes.Add(Theme);
		}
	}
	if(Themes.IsEmpty())
	{
		return FReply::Handled();
	}

	const FScopedTransaction Transaction(LOCTEXT("SortWidgetThemeStyles", "Sort Widget Theme Styles"));
	for(UWidgetTheme* Theme : Themes)
	{
		Theme->Modify();
		Theme->SortStyles();
		Theme->MarkPackageDirty();
	}
	if(DetailBuilder)
	{
		DetailBuilder->ForceRefreshDetails();
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
