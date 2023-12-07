// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/Widget/SModuleListItemWidget.h"

#include "SlateOptMacros.h"
#include "Main/Base/ModuleBase.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SModuleListItemWidget::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	Item = InArgs._Item;
	STableRow::Construct
	(
		STableRow::FArguments()
		.Style(FAppStyle::Get(), "SceneOutliner.TableViewRow")
		.ShowWires(false)
		.Content()
		[
			SNew(STextBlock)
			.Text_Lambda([this](){ return FText::FromString(FString::Printf(TEXT("%s"), *Item->Module->GetModuleDisplayName().ToString())); })
		], 
		InOwnerTableView
	);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
