// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Widget/SProcedureListItemWidget.h"

#include "SlateOptMacros.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SProcedureListItemWidget::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
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
			.Text_Lambda([this](){ return FText::FromString(FString::Printf(TEXT("%d.%s"), Item->Procedure->ProcedureIndex + 1, *Item->Procedure->ProcedureDisplayName.ToString())); })
			.HighlightText_Lambda([this](){ return ListWidget->ActiveFilterText; })
		],
		InOwnerTableView
	);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
