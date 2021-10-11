// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/SProcedureListItemWidget.h"

#include "SlateOptMacros.h"
#include "Procedure/Base/ProcedureBase.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SProcedureListItemWidget::Construct(const FArguments&, const TSharedPtr<const FProcedureListItem>& InItem)
{
	ChildSlot
	[
		SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
        .Padding(2.f)
        [
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text_Lambda([InItem](){ return InItem->Procedure->ProcedureDisplayName; })
			]
        ]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION
