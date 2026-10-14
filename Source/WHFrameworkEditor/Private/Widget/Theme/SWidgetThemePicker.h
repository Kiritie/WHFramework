#pragma once

#include "Widgets/SCompoundWidget.h"

class UWidgetTheme;

DECLARE_DELEGATE_OneParam(FWidgetThemeSelected, UWidgetTheme*);

class SWidgetThemePicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWidgetThemePicker) {}
	SLATE_ATTRIBUTE(UWidgetTheme*, SelectedTheme)
	SLATE_EVENT(FWidgetThemeSelected, OnThemeSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> BuildMenu();
	TAttribute<UWidgetTheme*> SelectedTheme;
	FWidgetThemeSelected OnThemeSelected;
};
