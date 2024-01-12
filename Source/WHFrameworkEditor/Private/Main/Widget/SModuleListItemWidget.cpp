// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/Widget/SModuleListItemWidget.h"

#include "SlateOptMacros.h"
#include "Main/Base/ModuleBase.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SModuleListItemWidget::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	Item = InArgs._Item;
	ListWidget = InArgs._ListWidget;
	
	STableRow::Construct
	(
		STableRow::FArguments()
		.Style(FAppStyle::Get(), "SceneOutliner.TableViewRow")
		.ShowWires(false)
		.Content()
		[
			SNew(STextBlock)
			.Text_Lambda([this](){ return FText::FromString(FString::Printf(TEXT("%s"), *Item->Module->GetModuleDisplayName().ToString())); })
			.ColorAndOpacity_Lambda([this](){ return !Item->Module->IsModuleRequired() ?  FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f)) : FSlateColor(FLinearColor(1.f, 0.5f, 0.2f)); })
			.HighlightText_Lambda([this](){ return ListWidget->ActiveFilterText; })
		],
		InOwnerTableView
	);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
