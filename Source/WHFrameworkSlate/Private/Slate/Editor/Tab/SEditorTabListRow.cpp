// Fill out your copyright notice in the Description page of Project Settings.


#include "Slate/Editor/Tab/SEditorTabListRow.h"

#include "SlateOptMacros.h"
#include "Slate/Editor/Tab/SEditorTabLabel.h"
#include "Slate/Editor/Misc/SEditorSplitLine.h"
#include "Slate/Editor/Tab/SEditorTabLabel_UnderLine.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SEditorTabListRow::SEditorTabListRow(): bKeepOneActivated(false)
{
}

void SEditorTabListRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	TabListItem = InArgs._TabListItem;
	bKeepOneActivated = InArgs._bKeepOneActivated;
	
	STableRow::Construct
	(
		STableRow::FArguments()
		.Style(InArgs._Style)
		.ShowWires(false)
		.Content()
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(InArgs._Padding)
			.AutoHeight()
			[
				InArgs._bHasUnderLine ? 
				SNew(SEditorTabLabel_UnderLine)
				.Icon_Lambda([this](){ return TabListItem->TabIcon ? (IsSelected() ? &TabListItem->TabIcon->ActiveBrush : &TabListItem->TabIcon->InactiveBrush) : nullptr; })
				.Label(FText::FromString(TabListItem->TabName)) :
				SNew(SEditorTabLabel)
				.Icon_Lambda([this](){ return TabListItem->TabIcon ? (IsSelected() ? &TabListItem->TabIcon->ActiveBrush : &TabListItem->TabIcon->InactiveBrush) : nullptr; })
				.Label(FText::FromString(TabListItem->TabName))
			]

			+SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.AutoHeight()
			[
				SNew(SEditorSplitLine)
				.Height(2.f)
				.Color(FSlateColor(FLinearColor(0.f, 0.4f, 0.95f)))
				.Visibility_Lambda([this](){ return !TabListItem->TabIcon && IsSelected() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
			]
		],
		InOwnerTableView
	);
}

FReply SEditorTabListRow::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TSharedRef< ITypedTableView<TSharedPtr<FEditorTabListItem>> > OwnerTable = OwnerTablePtr.Pin().ToSharedRef();
	bChangedSelectionOnMouseDown = false;
	bDragWasDetected = false;

	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		const ESelectionMode::Type SelectionMode = GetSelectionMode();
		if (SelectionMode != ESelectionMode::None)
		{
			if (const TSharedPtr<FEditorTabListItem>* MyItemPtr = GetItemForThis(OwnerTable))
			{
				const TSharedPtr<FEditorTabListItem>& MyItem = *MyItemPtr;
				const bool bIsSelected = OwnerTable->Private_IsItemSelected(MyItem);

				if (bIsSelected && OwnerTable->GetSelectedItems().Num() == 1 && bKeepOneActivated)
				{
					return FReply::Unhandled();
				}

				if (SelectionMode == ESelectionMode::Multi)
				{
					OwnerTable->Private_SetItemSelection(MyItem, !bIsSelected, true);
					bChangedSelectionOnMouseDown = true;
					if (SignalSelectionMode == ETableRowSignalSelectionMode::Instantaneous)
					{
						OwnerTable->Private_SignalSelectionChanged(ESelectInfo::OnMouseClick);
					}
				}

				if ((bAllowPreselectedItemActivation || !bIsSelected) && !bChangedSelectionOnMouseDown)
				{
					OwnerTable->Private_ClearSelection();
					OwnerTable->Private_SetItemSelection(MyItem, true, true);
					bChangedSelectionOnMouseDown = true;
					if (SignalSelectionMode == ETableRowSignalSelectionMode::Instantaneous)
					{
						OwnerTable->Private_SignalSelectionChanged(ESelectInfo::OnMouseClick);
					}
				}

				return FReply::Handled()
				       .DetectDrag(SharedThis(this), EKeys::LeftMouseButton)
				       .SetUserFocus(OwnerTable->AsWidget(), EFocusCause::Mouse)
				       .CaptureMouse(SharedThis(this));
			}
		}
	}

	return FReply::Unhandled();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
