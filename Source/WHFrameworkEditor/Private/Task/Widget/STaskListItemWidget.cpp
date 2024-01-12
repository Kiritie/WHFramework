// Fill out your copyright notice in the Description page of Project Settings.


#include "Task/Widget/STaskListItemWidget.h"

#include "SlateOptMacros.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void STaskListItemWidget::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
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
			.Text_Lambda([this](){ return FText::FromString(FString::Printf(TEXT("%d.%s"), Item->Task->TaskIndex + 1, *Item->Task->TaskDisplayName.ToString())); })
			.ColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f)))
			.HighlightText_Lambda([this](){ return ListWidget->ActiveFilterText; })
		],
		InOwnerTableView
	);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION