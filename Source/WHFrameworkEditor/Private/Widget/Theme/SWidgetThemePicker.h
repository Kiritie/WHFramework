#pragma once

#include "Widgets/SCompoundWidget.h"

class UWidgetTheme;

DECLARE_DELEGATE_OneParam(FWidgetThemeSelected, UWidgetTheme*);

class SWidgetThemePicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWidgetThemePicker) : _AllowProjectDefault(true) {}
	SLATE_ATTRIBUTE(UWidgetTheme*, SelectedTheme)
	SLATE_ARGUMENT(bool, AllowProjectDefault)
	SLATE_EVENT(FWidgetThemeSelected, OnThemeSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> BuildMenu();
	TAttribute<UWidgetTheme*> SelectedTheme;
	FWidgetThemeSelected OnThemeSelected;
	bool bAllowProjectDefault = true;
};
