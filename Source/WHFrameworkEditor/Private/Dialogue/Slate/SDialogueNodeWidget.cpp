#include "Dialogue/Slate/SDialogueNodeWidget.h"
#include "CoreMinimal.h"
#include "Dialogue/DialogueEditor.h"
#include "Dialogue/Slate/SDialogueViewportWidget.h"
#include "Dialogue/Style/DialogueEditorStyle.h"
#include "Dialogue/Base/DialogueAsset.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Editor/UnrealEd/Public/ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "SDialogueViewportWidget"

void SDialogueNodeWidget::Construct(const FArguments& InArgs)
{
	Id = InArgs._Id;
	NodeIndex = InArgs._NodeIndex;
	Dialogue = InArgs._Dialogue; // for styles, TD: move somewhere?
	Owner = InArgs._Owner;
	SetVisibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &SDialogueNodeWidget::GetNodeVisibility)));

	ChildSlot
	[
		SNew(SOverlay)
			/*
			* NodeImage
			*/
			+ SOverlay::Slot() // the node itself
			[
				SNew(SImage)
				.Image(this, &SDialogueNodeWidget::GetNodeStyle)
				.Visibility(EVisibility::Visible)
			]

			/*
			* a Spacer that gives a minimum size to the node
			*/
			+ SOverlay::Slot()
			.VAlign(VAlign_Top)
			[
				SNew(SSpacer)
				.Size(TAttribute<FVector2D>::Create(TAttribute<FVector2D>::FGetter::CreateSP(Owner, &SDialogueViewportWidget::GetNodeMinSize)))
			]

			/*
			* horizontal box containing top left icons
			*/
			+ SOverlay::Slot()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Left)
			.Padding(TAttribute<FMargin>::Create(TAttribute<FMargin>::FGetter::CreateSP(Owner, &SDialogueViewportWidget::GetLeftCornerPadding))) //.Padding(5, 0, 0, 0) at Zoom 1:1
			[
				SNew(SHorizontalBox)
				.RenderTransform(TAttribute<TOptional<FSlateRenderTransform>>::Create(TAttribute<TOptional<FSlateRenderTransform>>::FGetter::CreateSP(Owner, &SDialogueViewportWidget::GetIconScale)))
				.RenderTransformPivot(FVector2D(0, 0))
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(5, 3, 0, 0)
				[
					SNew(SImage)
					.Image(FDialogueEditorStyle::Get()->GetBrush("EventIcon"))
					.ColorAndOpacity(FLinearColor(246.0f / 255.0f, 207.0f / 255.0f, 6.0f / 255.0f, 1.0f))
					.Visibility(this, &SDialogueNodeWidget::GetEventIconVisibility)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(5, 3, 0, 0)
				[
					SNew(SImage)
					.Image(FDialogueEditorStyle::Get()->GetBrush("ConditionIcon"))
					.ColorAndOpacity(FLinearColor(106.0f / 255.0f, 221.0f / 255.0f, 214.0f / 255.0f, 1.0f)) // FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)
					.Visibility(this, &SDialogueNodeWidget::GetConditionIconVisibility)
				]
			]

			/*
			* horizontal box containing top right icons
			*/
			+ SOverlay::Slot()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Right)
			.Padding(0, 0, 0, 0) // left top right bottom
			[
				SNew(SHorizontalBox)
				.RenderTransform(TAttribute<TOptional<FSlateRenderTransform>>::Create(TAttribute<TOptional<FSlateRenderTransform>>::FGetter::CreateSP(Owner, &SDialogueViewportWidget::GetIconScale)))
				.RenderTransformPivot(FVector2D(1, 0))
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.FillWidth(1)
				.Padding(0, 3, 10, 0)
				[
					SNew(SImage)
					.Image(FDialogueEditorStyle::Get()->GetBrush("VoiceIcon"))
					.ColorAndOpacity(FLinearColor(106.0f / 255.0f, 174.0f / 255.0f, 101.0f / 255.0f, 1.0f))
					.Visibility(this, &SDialogueNodeWidget::GetSoundIconVisibility)
				]
			]

			/*
			* text block - displayed only when out of Editing mode
			*/
			+ SOverlay::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(TAttribute<FMargin>(this, &SDialogueNodeWidget::GetTextMargin)) // left top right bottom
			[
				SAssignNew(NodeTextBlock, STextBlock)
				.Justification(ETextJustify::Left)
				.Text(this, &SDialogueNodeWidget::GetNodeText)
				.Visibility(this, &SDialogueNodeWidget::GetTextBlockVisibility) // @TODO: make a getter?
				.WrapTextAt(TAttribute<float>::Create(TAttribute<float>::FGetter::CreateSP(Owner, &SDialogueViewportWidget::GetNodeTextWrapLength))) // 250 by default at 1:1 zoom level
				.Font(TAttribute<FSlateFontInfo>::Create(TAttribute<FSlateFontInfo>::FGetter::CreateSP(Owner, &SDialogueViewportWidget::GetNodeFont)))
			]

			/*
			* editable text - displayed only when in Editing mode
			*/
			+ SOverlay::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(TAttribute<FMargin>(this, &SDialogueNodeWidget::GetTextMargin))
			[
				SAssignNew(NodeField, SMultiLineEditableText)
				.Text(this, &SDialogueNodeWidget::GetFieldText)
				.Visibility(this, &SDialogueNodeWidget::GetTextFieldVisibility)
				.WrapTextAt(TAttribute<float>::Create(TAttribute<float>::FGetter::CreateSP(Owner, &SDialogueViewportWidget::GetNodeTextWrapLength))) // 250 by default at 1:1 zoom level
				.ModiferKeyForNewLine(EModifierKey::Shift)
				.OnTextCommitted(this, &SDialogueNodeWidget::TextCommited, Id)
				.SelectAllTextWhenFocused(true)
				.Font(TAttribute<FSlateFontInfo>::Create(TAttribute<FSlateFontInfo>::FGetter::CreateSP(Owner, &SDialogueViewportWidget::GetNodeFont)))
			]
	];
}

/*
* Performs culling. If the node isn't on screen, then it's collapsed. Boosts performance drammatically for dialogues with a large number of nodes.
*/
EVisibility SDialogueNodeWidget::GetNodeVisibility() const
{
	return isVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

FMargin SDialogueNodeWidget::GetTextMargin() const
{
	if (!isVisible)
	{
		return FMargin(0, 0, 0, 0);
	}

	float marg10 = 10 * Owner->GetZoomAmount();
	float marg17 = 16 * Owner->GetZoomAmount();

	if (Owner->GetZoomLevel() <= 4)
	{
		return FMargin(0, 0, 0, 0);
	}

	if (Owner->GetZoomLevel() > 6 && (Dialogue->Data[NodeIndex].Events.Num() > 0 || Dialogue->Data[NodeIndex].Conditions.Num() > 0 || Dialogue->Data[NodeIndex].Sound || Dialogue->Data[NodeIndex].DialogueWave))
	{
		return FMargin(marg10, marg17, marg10, marg10);
	}
	else
	{
		return FMargin(marg10, marg10, marg10, marg10);
	}
}

EVisibility SDialogueNodeWidget::GetTextFieldVisibility() const
{
	if (!isVisible)
	{
		return EVisibility::Collapsed;
	}

	if (Owner->GetZoomLevel() <= 6)
	{
		return EVisibility::Collapsed;
	}

	return (Owner->EditTextNode == Id) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SDialogueNodeWidget::GetEventIconVisibility() const
{
	if (!isVisible)
	{
		return EVisibility::Collapsed;
	}

	if (Owner->GetZoomLevel() <= 6)
	{
		return EVisibility::Collapsed;
	}

	return Dialogue->Data[NodeIndex].Events.Num() > 0 ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
}

EVisibility SDialogueNodeWidget::GetConditionIconVisibility() const
{
	if (!isVisible)
	{
		return EVisibility::Collapsed;
	}

	if (Owner->GetZoomLevel() <= 6)
	{
		return EVisibility::Collapsed;
	}

	return Dialogue->Data[NodeIndex].Conditions.Num() > 0 ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
}

EVisibility SDialogueNodeWidget::GetSoundIconVisibility() const
{
	if (!isVisible)
	{
		return EVisibility::Collapsed;
	}

	if (Owner->GetZoomLevel() <= 6)
	{
		return EVisibility::Collapsed;
	}

	return (Dialogue->Data[NodeIndex].Sound || Dialogue->Data[NodeIndex].DialogueWave) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
}

EVisibility SDialogueNodeWidget::GetTextBlockVisibility() const
{
	if (!isVisible)
	{
		return EVisibility::Collapsed;
	}

	if (Owner->GetZoomLevel() <= 4)
	{
		return EVisibility::Collapsed;
	}

	if (Owner->GetZoomLevel() <= 6)
	{
		return EVisibility::Hidden;
	}

	return (Owner->EditTextNode == Id) ? EVisibility::Collapsed : EVisibility::Visible;
}

void SDialogueNodeWidget::TextCommited(const FText& NewText, ETextCommit::Type CommitInfo, int32 id)
{
	if (NewText.EqualTo(Dialogue->Data[NodeIndex].Text)) return;
	const FScopedTransaction Transaction(LOCTEXT("TextCommited", "Edited Node Text"));
	Dialogue->Modify();
	Dialogue->Data[NodeIndex].Text = NewText;
	Owner->EditTextNode = -1;
	Owner->DialogueEditorPtr.Pin()->refreshDetails = true;
}

int32 SDialogueNodeWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void SDialogueNodeWidget::Tick(const FGeometry & AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	NodeSize = AllottedGeometry.GetLocalSize(); //TD: is it possible to bind it?
}

FReply SDialogueNodeWidget::OnMouseButtonDoubleClick(const FGeometry & InMyGeometry, const FPointerEvent & InMouseEvent)
{
	bJustDoubleClicked = true;
	return FReply::Unhandled();
}

void SDialogueNodeWidget::OnMouseEnter(const FGeometry & MyGeometry, const FPointerEvent & MouseEvent)
{
	// if mouse is over a node widget after we stopped linkingAndPanning, try to link the node
	if (Owner->GetEditingState()->isLinking && !Owner->isLinkingAndCapturing)
	{
		const int32 LinkingFromIndex = Owner->FindNodeIndex(Owner->GetEditingState()->LinkingFromNodeId);
		if (!Dialogue->Data.IsValidIndex(LinkingFromIndex))
		{
			Owner->GetEditingState()->isLinking = false;
			return;
		}
		// we shouldn't link a node to itself
		if (Id != Owner->GetEditingState()->LinkingFromNodeId)
		{
			// and we shouldn't link a node that's already linked
			if (!Dialogue->Data[LinkingFromIndex].Links.Contains(Id))
			{
				const FScopedTransaction Transaction(LOCTEXT("AddLink", "Add Link"));
				Dialogue->Modify();

				Dialogue->Data[LinkingFromIndex].Links.Add(Id);
				SortParentsLinks();
			}
		}
		// whatever node we entered, we stop linking
		Owner->GetEditingState()->isLinking = false;
	}
}

const FSlateBrush* SDialogueNodeWidget::GetNodeStyle() const
{
	if (Id == 0) return FDialogueEditorStyle::Get()->GetBrush("StartNodeStyle");

	return Dialogue->Data[NodeIndex].isPlayer ? FDialogueEditorStyle::Get()->GetBrush("PlayerNodeStyle") : FDialogueEditorStyle::Get()->GetBrush("NpcNodeStyle");
}

FText SDialogueNodeWidget::GetNodeText() const
{
	if (!isVisible)
	{
		return FText::FromString("");
	}

	FText NodeText = Dialogue->Data[NodeIndex].Text;

	int maxTextLength = 300; //@TODO: change 300 to user's choice

	if (NodeText.ToString().Len() > maxTextLength)
	{
		FString Output;

		Output += NodeText.ToString();
		Output.RemoveAt(maxTextLength, NodeText.ToString().Len() - maxTextLength, EAllowShrinking::Yes);
		Output += TEXT("...");

		return FText::FromString(Output);
	}

	return NodeText;
}

FText SDialogueNodeWidget::GetFieldText() const
{
	if (!isVisible)
	{
		return FText::FromString("");
	}

	return Dialogue->Data[NodeIndex].Text;
}

FReply SDialogueNodeWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FReply Reply = FReply::Handled();

	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		Reply.DetectDrag(SharedThis(this), EKeys::RightMouseButton);
	}
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Reply.DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
	}

	return Reply;
}

FReply SDialogueNodeWidget::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton) && !Owner->GetEditingState()->isLinking)
	{
		Owner->isPanning = true;
	}
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		if (Owner->bBreakingLinksMode)
			Owner->bBreakingLinksMode = false;

		const FScopedTransaction Transaction(LOCTEXT("DragNodes", "Drag Nodes"));
		Dialogue->Modify();

		// offset = local coords of the center of the node - local coords of the mouse click (inside the node)
		Owner->draggingOffset = FVector2D(MyGeometry.GetLocalSize().X / 2, MyGeometry.GetLocalSize().Y / 2) - MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		Owner->StartDraggingIndex(NodeIndex);
	}
	return FReply::Handled();
}

FReply SDialogueNodeWidget::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	/* Left mouse button UP */
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if ((bJustDoubleClicked || (Owner->SelectedNodes.Num() == 1 && Owner->SelectedNodes.Contains(Id) && NodeTextBlock->IsHovered())) && Id != 0)
		{
			bJustDoubleClicked = false;

			// Don't enter Edit mode if we're at -6 zoom or lower.
			if (Owner->GetZoomLevel() > 6)
			{
				NodeField->SetText(GetFieldText());
				Owner->EditTextNode = Id;
				FSlateApplication::Get().SetKeyboardFocus(NodeField);
			}

			return FReply::Handled();
		}

		if (Owner->bBreakingLinksMode)
		{
			// Breaking links between Id and Owner->breakingLinksFromId
			BreakLinksWithNode();
			return FReply::Handled();
		}

		return FReply::Unhandled();
	}
	/* Right mouse button UP */
	else if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (Owner->bBreakingLinksMode)
		{
			Owner->bBreakingLinksMode = false;
		}

		if (!Owner->SelectedNodes.Contains(Id))
		{
			Owner->SelectNodes(Id);
			Owner->GetEditingState()->CurrentNodeId = Id;
			Owner->ForceRefresh();
		}

		if (Owner->GetEditingState()->isLinking) // if RMB UP on a node when linking, cancel linking
		{
			Owner->GetEditingState()->isLinking = false;
			return FReply::Handled();
		}

		FMenuBuilder MenuBuilder(true, NULL);

		FUIAction AddLinkAction(FExecuteAction::CreateSP(this, &SDialogueNodeWidget::OnAddLink));
		FUIAction AddPCAnswerAction(FExecuteAction::CreateSP(this, &SDialogueNodeWidget::OnAddPcAnswer));
		FUIAction AddNPCAnswerAction(FExecuteAction::CreateSP(this, &SDialogueNodeWidget::OnAddNpcAnswer));
		FUIAction BreakOutLinksAction(FExecuteAction::CreateSP(this, &SDialogueNodeWidget::OnBreakOutLinks));
		FUIAction BreakInLinksAction(FExecuteAction::CreateSP(this, &SDialogueNodeWidget::OnBreakInLinks));
		FUIAction ChangePcNpcAction(FExecuteAction::CreateSP(this, &SDialogueNodeWidget::OnChangePcNpc));
		FUIAction DeleteAction(FExecuteAction::CreateSP(Owner, &SDialogueViewportWidget::DeleteOneNode, Id));
		FUIAction DeleteAllAction(FExecuteAction::CreateSP(Owner, &SDialogueViewportWidget::DeleteSelected));
		FUIAction BreakLinksModeAction(FExecuteAction::CreateSP(this, &SDialogueNodeWidget::BreakLinksMode));

		MenuBuilder.BeginSection(NAME_None, NSLOCTEXT("PropertyView", "ExpansionHeading", "Dialogue"));

		// can't give a PC answer to a previous PC node
		if (!Dialogue->Data[NodeIndex].isPlayer)
		{
			MenuBuilder.AddMenuEntry(NSLOCTEXT("PropertyView", "AddPcAnswer", "Add PC Answer"), NSLOCTEXT("PropertyView", "AddPcAnswer_ToolTip", "Adds a child node (PC Answer)"), FSlateIcon(), AddPCAnswerAction);
		}

		MenuBuilder.AddMenuEntry(NSLOCTEXT("PropertyView", "AddNpcAnswer", "Add NPC Answer"), NSLOCTEXT("PropertyView", "AddNpcAnswer_ToolTip", "Adds a child node (NPC Answer)"), FSlateIcon(), AddNPCAnswerAction);
		MenuBuilder.AddMenuEntry(NSLOCTEXT("PropertyView", "AddLink", "Add Link"), NSLOCTEXT("PropertyView", "AddLink_ToolTip", "Usage: left click on the node you want to link"), FSlateIcon(), AddLinkAction);

		if (Owner->SelectedNodes.Num() == 1 && Owner->SelectedNodes[0] == Id)
		{
			MenuBuilder.AddMenuEntry(NSLOCTEXT("PropertyView", "BreakLinks", "Break Links With Node..."), NSLOCTEXT("PropertyView", "BreakLinks_ToolTip", "Usage: left click on the node you want to break links with"), FSlateIcon(), BreakLinksModeAction);
		}

		MenuBuilder.AddMenuEntry(NSLOCTEXT("PropertyView", "BreakOutLinks", "Break Outgoing Links"), NSLOCTEXT("PropertyView", "BreakOutLinks_ToolTip", "Breaks all outgoing links"), FSlateIcon(), BreakOutLinksAction);
		MenuBuilder.AddMenuEntry(NSLOCTEXT("PropertyView", "BreakInLinks", "Break Incoming Links"), NSLOCTEXT("PropertyView", "BreakInLinks_ToolTip", "Breaks all incoming links"), FSlateIcon(), BreakInLinksAction);

		if (Id != 0)
		{
			if (Dialogue->Data[NodeIndex].isPlayer)
			{
				MenuBuilder.AddMenuEntry(NSLOCTEXT("PropertyView", "ChangeToNpc", "Change to NPC"), NSLOCTEXT("PropertyView", "ChangeToNpc_ToolTip", "Changes node to an NPC answer"), FSlateIcon(), ChangePcNpcAction);
			}
			else
			{
				MenuBuilder.AddMenuEntry(NSLOCTEXT("PropertyView", "ChangeToPc", "Change to PC"), NSLOCTEXT("PropertyView", "ChangeToPc_ToolTip", "Changes node to a PC answer"), FSlateIcon(), ChangePcNpcAction);
			}
		}

		//can't delete start node
		if (Id != 0)
		{
			MenuBuilder.AddMenuEntry(NSLOCTEXT("PropertyView", "Delete", "Delete Node"), NSLOCTEXT("PropertyView", "Delete_ToolTip", "Deletes this node"), FSlateIcon(), DeleteAction);
		}

		if (Owner->SelectedNodes.IsValidIndex(0) && Owner->SelectedNodes.IsValidIndex(1)) // if multiple are selected
		{
			MenuBuilder.AddMenuEntry(NSLOCTEXT("PropertyView", "DeleteSelected", "Delete Selected Nodes"), NSLOCTEXT("PropertyView", "DeleteAll_ToolTip", "Deletes selected nodes"), FSlateIcon(), DeleteAllAction);
		}

		MenuBuilder.EndSection();

		FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
		FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, MenuBuilder.MakeWidget(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect::ContextMenu);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SDialogueNodeWidget::BreakLinksMode()
{
	Owner->bBreakingLinksMode = true;
	Owner->breakingLinksFromId = Id;
}

void SDialogueNodeWidget::BreakLinksWithNode()
{
	Owner->bBreakingLinksMode = false;
	const int32 BreakingFromNodeIndex = Owner->FindNodeIndex(Owner->breakingLinksFromId);
	if (!Dialogue->Data.IsValidIndex(BreakingFromNodeIndex)) return;

	// if the two nodes aren't linked in any way or we clicked on the same node, just return
	if (Owner->breakingLinksFromId == Id || (!Dialogue->Data[NodeIndex].Links.Contains(Owner->breakingLinksFromId) && !Dialogue->Data[BreakingFromNodeIndex].Links.Contains(Id)))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("BreakLinksWithNode", "Break Links With Node..."));
	Dialogue->Modify();

	Dialogue->Data[NodeIndex].Links.RemoveAll([this](const int32 param1)
	{
		return (param1 == Owner->breakingLinksFromId);
	});

	Dialogue->Data[BreakingFromNodeIndex].Links.RemoveAll([this](const int32 param1)
	{
		return (param1 == Id);
	});
}

void SDialogueNodeWidget::OnChangePcNpc()
{
	if (Dialogue->Data[NodeIndex].isPlayer)
	{
		const FScopedTransaction Transaction(LOCTEXT("ChangePcNpc", "Switch Node To NPC"));
		Dialogue->Modify();
	}
	else
	{
		const FScopedTransaction Transaction(LOCTEXT("ChangeNpcPc", "Switch Node To PC"));
		Dialogue->Modify();
	}

	Dialogue->Data[NodeIndex].isPlayer = !Dialogue->Data[NodeIndex].isPlayer;
}

void SDialogueNodeWidget::OnBreakOutLinks()
{
	const FScopedTransaction Transaction(LOCTEXT("BreakOutLinks", "Break Outgoing Links"));
	Dialogue->Modify();

	Dialogue->Data[NodeIndex].Links.Empty();
}

void SDialogueNodeWidget::OnBreakInLinks()
{
	const FScopedTransaction Transaction(LOCTEXT("BreakInLinks", "Break Incoming Links"));
	Dialogue->Modify();
	BreakInLinks();
}

void SDialogueNodeWidget::BreakInLinks()
{
	int i = 0;
	for (auto Node : Dialogue->Data)
	{
		Dialogue->Data[i].Links.RemoveAll([this](const int32 param1)
		{
			return (param1 == Id);
		});
		i++;
	}
}

void SDialogueNodeWidget::OnAddPcAnswer()
{
	const FScopedTransaction Transaction(LOCTEXT("AddPcAnswer", "Add PC Answer"));
	Dialogue->Modify();

	int index = NodeIndex;

	FDialogueNode NewNode;

	NewNode.id = Dialogue->NextNodeId;
	NewNode.isPlayer = true;

	if (Dialogue->Data[index].Links.Num() == 0)
	{
		NewNode.Coordinates.X = Dialogue->Data[index].Coordinates.X;
		NewNode.Coordinates.Y = Dialogue->Data[index].Coordinates.Y + (NodeSize.Y + Owner->NodeWidgets[0]->NodeSize.Y) / 2.f / Owner->GetZoomAmount() + 75.f; // half the size of current widget + half the size of first widget + desired distance
	}
	else
	{
		int32 lastIndex;
		int32 lastId;
		int32 linkToLastId = Dialogue->Data[index].Links.FindLastByPredicate([&](const int32 param1)
		{
			const int32 TempIndex = Owner->FindNodeIndex(param1);
			return Dialogue->Data.IsValidIndex(TempIndex) && Dialogue->Data[TempIndex].Coordinates.Y > Dialogue->Data[index].Coordinates.Y;
		});

		if (linkToLastId != INDEX_NONE)
		{
			lastId = Dialogue->Data[index].Links[linkToLastId];
			lastIndex = Owner->FindNodeIndex(lastId);

			NewNode.Coordinates.X = Dialogue->Data[lastIndex].Coordinates.X + (Owner->NodeWidgets[lastIndex]->NodeSize.X + Owner->NodeWidgets[0]->NodeSize.X) / 2.f / Owner->GetZoomAmount() + 50.f; // half the size of the preceding widget + half the size of the new widget + desired distance
			NewNode.Coordinates.Y = Dialogue->Data[lastIndex].Coordinates.Y;
		}
		else
		{	// same as if (Dialogue->Data[index].Links.Num() == 0)
			NewNode.Coordinates.X = Dialogue->Data[index].Coordinates.X;
			NewNode.Coordinates.Y = Dialogue->Data[index].Coordinates.Y + (NodeSize.Y + Owner->NodeWidgets[0]->NodeSize.Y) / 2.f / Owner->GetZoomAmount() + 75.f; // half the size of current widget + half the size of first widget + desired distance
		}
	}

	Dialogue->NextNodeId++;
	Dialogue->Data.Add(NewNode);
	Dialogue->Data[index].Links.Add(NewNode.id);
	Owner->SpawnNodes(NewNode.id);
}

void SDialogueNodeWidget::OnAddNpcAnswer()
{
	const FScopedTransaction Transaction(LOCTEXT("AddNpcAnswer", "Add NPC Answer"));
	Dialogue->Modify();

	int index = NodeIndex;

	FDialogueNode NewNode;

	NewNode.id = Dialogue->NextNodeId;
	NewNode.isPlayer = false;

	if (Dialogue->Data[index].Links.Num() == 0)
	{
		NewNode.Coordinates.X = Dialogue->Data[index].Coordinates.X;
		NewNode.Coordinates.Y = Dialogue->Data[index].Coordinates.Y + (NodeSize.Y + Owner->NodeWidgets[0]->NodeSize.Y) / 2.f / Owner->GetZoomAmount() + 75.f; // half the size of current widget + half the size of first widget + desired distance
	}
	else
	{
		int32 lastIndex;
		int32 lastId;
		int32 linkToLastId = Dialogue->Data[index].Links.FindLastByPredicate([&](const int32 param1)
		{
			const int32 TempIndex = Owner->FindNodeIndex(param1);
			return Dialogue->Data.IsValidIndex(TempIndex) && Dialogue->Data[TempIndex].Coordinates.Y > Dialogue->Data[index].Coordinates.Y;
		});

		if (linkToLastId != INDEX_NONE)
		{
			lastId = Dialogue->Data[index].Links[linkToLastId];
			lastIndex = Owner->FindNodeIndex(lastId);

			NewNode.Coordinates.X = Dialogue->Data[lastIndex].Coordinates.X + (Owner->NodeWidgets[lastIndex]->NodeSize.X + Owner->NodeWidgets[0]->NodeSize.X) / 2.f / Owner->GetZoomAmount() + 50.f; // half the size of the preceding widget + half the size of the new widget + desired distance
			NewNode.Coordinates.Y = Dialogue->Data[lastIndex].Coordinates.Y;
		}
		else
		{	// same as if (Dialogue->Data[index].Links.Num() == 0)
			NewNode.Coordinates.X = Dialogue->Data[index].Coordinates.X;
			NewNode.Coordinates.Y = Dialogue->Data[index].Coordinates.Y + (NodeSize.Y + Owner->NodeWidgets[0]->NodeSize.Y) / 2.f / Owner->GetZoomAmount() + 75.f; // half the size of current widget + half the size of first widget + desired distance
		}
	}

	Dialogue->NextNodeId++;
	Dialogue->Data.Add(NewNode);
	Dialogue->Data[index].Links.Add(NewNode.id);
	Owner->SpawnNodes(NewNode.id);
}

void SDialogueNodeWidget::OnAddLink()
{
	Owner->GetEditingState()->isLinking = true;
	Owner->isLinkingAndCapturing = true;
	Owner->GetEditingState()->LinkingFromNodeId = Id;
	Owner->GetEditingState()->LinkingCoords = Dialogue->Data[NodeIndex].Coordinates;
	Owner->ForceSlateToStayAwake();
}

/* After moving a node, we have to re-sort its parents child-links, so that the replies are shown based on the nodes horizontal positionning.
* For example, if Node A is to the left of Node B, then in-game you will see the answer A before answer B.
*/
void SDialogueNodeWidget::SortParentsLinks()
{
	int32 i = 0;
	for (auto Node : Dialogue->Data)
	{
		int32 j = 0;
		for (auto Link : Node.Links)
		{
			if (Dialogue->Data[i].Links[j] == Id) // if a link to current node has been found, then perform a sort on the links
			{
				Dialogue->Data[i].Links.Sort([&](const int32 id1, const int32 id2) // resort them depending on their x coordinate
				{
					const int32 Index1 = Owner->FindNodeIndex(id1);
					const int32 Index2 = Owner->FindNodeIndex(id2);
					if (!Dialogue->Data.IsValidIndex(Index1)) return false;
					if (!Dialogue->Data.IsValidIndex(Index2)) return true;
					return Dialogue->Data[Index1].Coordinates.X < Dialogue->Data[Index2].Coordinates.X;
				});
				break;
			}
			j++;
		}
		i++;
	}
}

#undef LOCTEXT_NAMESPACE
