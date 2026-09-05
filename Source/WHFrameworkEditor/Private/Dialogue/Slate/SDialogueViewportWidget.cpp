#include "Dialogue/Slate/SDialogueViewportWidget.h"
#include "CoreMinimal.h"
#include "Dialogue/Slate/SDialogueNodeWidget.h"
#include "Dialogue/Style/DialogueEditorStyle.h"
#include "Dialogue/DialogueEditor.h"
#include "Dialogue/DialogueGraphClipboard.h"
#include "Editor/GraphEditor/Private/SGraphEditorImpl.h"
#include "Dialogue/Base/DialogueAsset.h"
#include "Dialogue/Base/DialogueConditionBase.h"
#include "Dialogue/Base/DialogueEventBase.h"
#include "Runtime/Slate/Public/Widgets/Layout/SConstraintCanvas.h"
#include "Editor/UnrealEd/Public/ScopedTransaction.h"
#include "GraphEditorSettings.h"
#include "Editor.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandList.h"
#include "EdGraphUtilities.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Sound/SoundBase.h"
#include "Sound/DialogueWave.h"
#include "JsonObjectConverter.h"
#include "UObject/NoExportTypes.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"

#define LOCTEXT_NAMESPACE "SDialogueViewportWidget"

struct FDialogueZoomLevelEntry
{
public:
	FDialogueZoomLevelEntry(float InZoomAmount, const FText& InDisplayText, EDialogueRenderingLOD::Type InLOD)
		: DisplayText(FText::Format(NSLOCTEXT("GraphEditor", "Zoom", "Zoom {0}"), InDisplayText))
		, ZoomAmount(InZoomAmount)
		, LOD(InLOD)
	{
	}

public:
	FText DisplayText;
	float ZoomAmount;
	EDialogueRenderingLOD::Type LOD;
};

struct FDialogueFixedZoomLevelsContainer : public FDialogueZoomLevelsContainer
{
	FDialogueFixedZoomLevelsContainer()
	{
		// Initialize zoom levels if not done already
		if (ZoomLevels.Num() == 0)
		{
			ZoomLevels.Reserve(20);
			ZoomLevels.Add(FDialogueZoomLevelEntry(0.100f, FText::FromString(TEXT("-12")), EDialogueRenderingLOD::LowestDetail));
			ZoomLevels.Add(FDialogueZoomLevelEntry(0.125f, FText::FromString(TEXT("-11")), EDialogueRenderingLOD::LowestDetail));
			ZoomLevels.Add(FDialogueZoomLevelEntry(0.150f, FText::FromString(TEXT("-10")), EDialogueRenderingLOD::LowestDetail));
			ZoomLevels.Add(FDialogueZoomLevelEntry(0.175f, FText::FromString(TEXT("-9")), EDialogueRenderingLOD::LowestDetail));
			ZoomLevels.Add(FDialogueZoomLevelEntry(0.200f, FText::FromString(TEXT("-8")), EDialogueRenderingLOD::LowestDetail));
			ZoomLevels.Add(FDialogueZoomLevelEntry(0.225f, FText::FromString(TEXT("-7")), EDialogueRenderingLOD::LowDetail));
			ZoomLevels.Add(FDialogueZoomLevelEntry(0.250f, FText::FromString(TEXT("-6")), EDialogueRenderingLOD::LowDetail));
			ZoomLevels.Add(FDialogueZoomLevelEntry(0.375f, FText::FromString(TEXT("-5")), EDialogueRenderingLOD::MediumDetail));
			ZoomLevels.Add(FDialogueZoomLevelEntry(0.500f, FText::FromString(TEXT("-4")), EDialogueRenderingLOD::MediumDetail));
			ZoomLevels.Add(FDialogueZoomLevelEntry(0.675f, FText::FromString(TEXT("-3")), EDialogueRenderingLOD::MediumDetail));
			ZoomLevels.Add(FDialogueZoomLevelEntry(0.750f, FText::FromString(TEXT("-2")), EDialogueRenderingLOD::DefaultDetail));
			ZoomLevels.Add(FDialogueZoomLevelEntry(0.875f, FText::FromString(TEXT("-1")), EDialogueRenderingLOD::DefaultDetail));
			ZoomLevels.Add(FDialogueZoomLevelEntry(1.000f, FText::FromString(TEXT("1:1")), EDialogueRenderingLOD::DefaultDetail));
			ZoomLevels.Add(FDialogueZoomLevelEntry(1.250f, FText::FromString(TEXT("+1")), EDialogueRenderingLOD::DefaultDetail));
			ZoomLevels.Add(FDialogueZoomLevelEntry(1.375f, FText::FromString(TEXT("+2")), EDialogueRenderingLOD::DefaultDetail));
			ZoomLevels.Add(FDialogueZoomLevelEntry(1.500f, FText::FromString(TEXT("+3")), EDialogueRenderingLOD::FullyZoomedIn));
			ZoomLevels.Add(FDialogueZoomLevelEntry(1.675f, FText::FromString(TEXT("+4")), EDialogueRenderingLOD::FullyZoomedIn));
			ZoomLevels.Add(FDialogueZoomLevelEntry(1.750f, FText::FromString(TEXT("+5")), EDialogueRenderingLOD::FullyZoomedIn));
			ZoomLevels.Add(FDialogueZoomLevelEntry(1.875f, FText::FromString(TEXT("+6")), EDialogueRenderingLOD::FullyZoomedIn));
			ZoomLevels.Add(FDialogueZoomLevelEntry(2.000f, FText::FromString(TEXT("+7")), EDialogueRenderingLOD::FullyZoomedIn));
		}
	}

	float GetZoomAmount(int32 InZoomLevel) const override
	{
		checkSlow(ZoomLevels.IsValidIndex(InZoomLevel));
		return ZoomLevels[InZoomLevel].ZoomAmount;
	}

	int32 GetNearestZoomLevel(float InZoomAmount) const override
	{
		for (int32 ZoomLevelIndex = 0; ZoomLevelIndex < GetNumZoomLevels(); ++ZoomLevelIndex)
		{
			if (InZoomAmount <= GetZoomAmount(ZoomLevelIndex))
			{
				return ZoomLevelIndex;
			}
		}

		return GetDefaultZoomLevel();
	}

	FText GetZoomText(int32 InZoomLevel) const override
	{
		checkSlow(ZoomLevels.IsValidIndex(InZoomLevel));
		return ZoomLevels[InZoomLevel].DisplayText;
	}

	int32 GetNumZoomLevels() const override
	{
		return ZoomLevels.Num();
	}

	int32 GetDefaultZoomLevel() const override
	{
		return 12;
	}

	EDialogueRenderingLOD::Type GetLOD(int32 InZoomLevel) const override
	{
		checkSlow(ZoomLevels.IsValidIndex(InZoomLevel));
		return ZoomLevels[InZoomLevel].LOD;
	}

	static TArray<FDialogueZoomLevelEntry> ZoomLevels;
};

const TCHAR* XSymbol = TEXT("\xD7");
TArray<FDialogueZoomLevelEntry> FDialogueFixedZoomLevelsContainer::ZoomLevels;

void SDialogueViewportWidget::Construct(const FArguments& InArgs, TSharedPtr<FDialogueEditor> InDialogueEditor)
{
	if (!ZoomLevels)
	{
		ZoomLevels = MakeUnique<FDialogueFixedZoomLevelsContainer>();
	}
	ZoomLevel = ZoomLevels->GetDefaultZoomLevel();
	PreviousZoomLevel = ZoomLevels->GetDefaultZoomLevel();
	ZoomLevelFade = FCurveSequence(0.0f, 1.0f);
	ZoomLevelFade.Play(this->AsShared());
	PostChangedZoom();
	UpdateZoomAffectedValues();

	Dialogue = InArgs._Dialogue;
	DialogueEditorPtr = InDialogueEditor;
	EditingState = InDialogueEditor->GetEditingState();
	ShowGraphStateOverlay = InArgs._ShowGraphStateOverlay;
	IsEditable = InArgs._IsEditable;

	ChildSlot
	[
		SNew(SBox)
		[
			SAssignNew(CanvasPanel, SConstraintCanvas)
		]
	];
	SpawnNodes();
}

void SDialogueViewportWidget::SpawnNodes(int32 IdToFocus)
{
	CanvasPanel->ClearChildren();
	NodeFSlots.Empty();
	NodeWidgets.Empty();
	isNodeVisible.Empty();
	NodeIdsIndexes.Empty();

	if (!Dialogue->Data.IsValidIndex(0)) // if we're opening the dataAsset for the first time, create the Start node
	{
		FDialogueNode NewNode;
		NewNode.id = 0;
		NewNode.Text = FText::FromString(TEXT("Start"));
		bFirstNodeRequiresRepositionning = true;
		Dialogue->NextNodeId = 1;
		Dialogue->Data.Add(NewNode);
	}

	// nodes themselves
	int i = 0;
	for (auto &Node : Dialogue->Data)
	{
		SConstraintCanvas::FSlot* tempSlot;
		CanvasPanel->AddSlot()
			.Offset(TAttribute<FMargin>::Create(TAttribute<FMargin>::FGetter::CreateSP(this, &SDialogueViewportWidget::GetPos, i)))
			.AutoSize(true)
			.Expose(tempSlot)
			[
				SAssignNew(tempWidgetReference, SDialogueNodeWidget)
				.Id(Node.id)
				.NodeIndex(i)
				.Dialogue(Dialogue)
				.Owner(this)
			];
		isNodeVisible.Add(true);
		NodeFSlots.Add(tempSlot);
		NodeWidgets.Add(tempWidgetReference);
		NodeIdsIndexes.Add(Node.id, i);
		i++;
	}

	// Bottom-right corner text indicating the type of tool: DIALOGUE
	CanvasPanel->AddSlot()
		.Anchors(FAnchors(1, 1, 1, 1))
		.AutoSize(true)
		.Alignment(FVector2D(1, 1))
		.Offset(FMargin(-10.0f, -10.0f))
		[
			SNew(STextBlock)
			.Visibility(EVisibility::HitTestInvisible)
			.TextStyle(FAppStyle::Get(), "Graph.CornerText")
			.Text(FText::FromString(TEXT("DIALOGUE")))
		];

	// Top-right corner text indicating PIE is active
	CanvasPanel->AddSlot()
		.Anchors(FAnchors(1, 0, 1, 0))
		.AutoSize(true)
		.Alignment(FVector2D(1, 0))
		.Offset(FMargin(-20.f, 20.f))
		[
			SNew(STextBlock)
			.Visibility(this, &SDialogueViewportWidget::IsSimulating)
			.TextStyle(FAppStyle::Get(), "Graph.SimulatingText")
			.Text(FText::FromString(TEXT("SIMULATING")))
		];

	// Indicator of current zoom level
	CanvasPanel->AddSlot()
		.Anchors(FAnchors(1, 0, 1, 0))
		.AutoSize(true)
		.Alignment(FVector2D(1, 0))
		.Offset(FMargin(-5.f, 5.f))
		[
			SNew(STextBlock)
			.TextStyle(FAppStyle::Get(), "Graph.ZoomText")
			.Text(this, &SDialogueViewportWidget::GetZoomText)
			.ColorAndOpacity(this, &SDialogueViewportWidget::GetZoomTextColorAndOpacity)
		];

	// if we just respawned nodes and we need to focus a node (because we just created it, for example)
	if (IdToFocus != 0)
	{
		SelectNodes(IdToFocus);
		EditingState->CurrentNodeId = IdToFocus;
		ForceRefresh();
	}

	shouldTickAllWidgets = true;
}

void SDialogueViewportWidget::Tick(const FGeometry & AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();
	FVector2D temp_coords = AllottedGeometry.AbsoluteToLocal(CursorPos) / GetZoomAmount() - panningOffset;
	//CoordsForPasting = FIntPoint(FMath::RoundToInt(temp_coords.X), FMath::RoundToInt(temp_coords.Y));

	FSlateRect PanelScreenSpaceRect = AllottedGeometry.GetLayoutBoundingRect();
	FSlateRect ScreenRectLocalAndZoomed = FSlateRect(AllottedGeometry.AbsoluteToLocal(FVector2D(PanelScreenSpaceRect.Left, PanelScreenSpaceRect.Top)) / GetZoomAmount() - panningOffset,
		AllottedGeometry.AbsoluteToLocal(FVector2D(PanelScreenSpaceRect.Right, PanelScreenSpaceRect.Bottom)) / GetZoomAmount() - panningOffset);
	CoordsForPasting = FIntPoint(
		FMath::RoundToInt(FMath::Clamp(temp_coords.X, ScreenRectLocalAndZoomed.Left, ScreenRectLocalAndZoomed.Right)),
		FMath::RoundToInt(FMath::Clamp(temp_coords.Y, ScreenRectLocalAndZoomed.Top, ScreenRectLocalAndZoomed.Bottom))
	);

	//FSlateRect PanelScreenSpaceRect = AllottedGeometry.GetLayoutBoundingRect();
	//CoordsForPasting = FIntPoint(
	//	FMath::RoundToInt(FMath::Clamp(temp_coords.X, PanelScreenSpaceRect.Left, PanelScreenSpaceRect.Right)),
	//	FMath::RoundToInt(FMath::Clamp(temp_coords.Y, PanelScreenSpaceRect.Top, PanelScreenSpaceRect.Bottom))
	//);

	CachedLocalSize = AllottedGeometry.GetLocalSize();

	if (!shouldTickAllWidgets)
	{
		for (int i = 0; i < NodeWidgets.Num(); i++)
		{
			/* If node is inside the viewport or near the viewport by 50 pixels.*/
			if ((NodeFSlots[i]->GetOffset().Left + NodeWidgets[i]->NodeSize.X > -50.f
					&& NodeFSlots[i]->GetOffset().Left - NodeWidgets[i]->NodeSize.X < CachedLocalSize.X + 50.f)
				&&
				((NodeFSlots[i]->GetOffset().Top + NodeWidgets[i]->NodeSize.Y > -50.f
					&& NodeFSlots[i]->GetOffset().Top - NodeWidgets[i]->NodeSize.Y < CachedLocalSize.Y + 50.f)))
			{
				if (isNodeVisible[i] == false)
				{
					isNodeVisible[i] = true;
					// Set a node's visible field to true. However, it'll only really become visible when it enters the bounds of the Viewport, and that's when it'll start ticking.
					NodeWidgets[i]->isVisible = true;
				}
			}
			else
			{
				if (isNodeVisible[i] == true)
				{
					isNodeVisible[i] = false;
					NodeWidgets[i]->isVisible = false;
				}
			}
		}
	}

	// When we just opened an empty dialogue for the first time, and the first node was just created, we have to position it properly.
	// We have to do it on the first tick, when AllottedGeometry is already initialized, so that we can place the node in the top center of the screen.
	if (bFirstNodeRequiresRepositionning)
	{
		bFirstNodeRequiresRepositionning = false;

		FVector2D TopLeftCoords = AllottedGeometry.AbsoluteToLocal(FVector2D(AllottedGeometry.GetLayoutBoundingRect().Left, AllottedGeometry.GetLayoutBoundingRect().Top));
		FVector2D BottomRightCoords = AllottedGeometry.AbsoluteToLocal(FVector2D(AllottedGeometry.GetLayoutBoundingRect().Right, AllottedGeometry.GetLayoutBoundingRect().Bottom));

		float HCoord = FMath::LerpStable(TopLeftCoords.X, BottomRightCoords.X, 0.5f);
		float VCoord = FMath::LerpStable(TopLeftCoords.Y, BottomRightCoords.Y, 0.3f);
		Dialogue->Data[0].Coordinates = FVector2D(HCoord, VCoord);
	}

	// Applying automatic panning when linking, dragging or selecting multiple, and the mouse is near the border.
	if ((isLinkingAndCapturing || draggedNodeIndex != -1 || isSelectingMultiple) && !AutoPanningDirection.IsZero())
	{
		panningOffset += AutoPanningDirection / GetZoomAmount() * InDeltaTime * 60;

		if (isSelectingMultiple)
		{
			selectionPanningOffset += AutoPanningDirection * InDeltaTime * 60;
		}

		float x = fmod(panningOffset.Y, 128.0f);
		float y = fmod(panningOffset.X, 128.0f);

		bgTop = -128 + x;
		bgLeft = -128 + y;
	}

	// Activate all widgets, tick them, disable them - this allows all widgets to update their size at the new zoom level even though they're culled and are beyond the Viewport.
	if (shouldTickAllWidgets)
	{
		int nodeIndex = 0;
		for (auto Node : Dialogue->Data)
		{
			NodeWidgets[nodeIndex]->isVisible = true;
			isNodeVisible[nodeIndex] = true;
			NodeWidgets[nodeIndex]->SetVisibility(EVisibility::Visible);
			nodeIndex++;
		}
		shouldTickAllWidgets = false;
		//TickWidgetsRecursively(AllottedGeometry, InCurrentTime, InDeltaTime);	 // no longer needed since 4.20?
	}
}

/*
* OnCursorQuery effectively acts as Tick, since we capture the mouse when we drag nodes.
* This provides us with the mouse coordinates, that Tick and OnMouseMove wouldn't (because we sometimes
* stop moving the mouse when we're at the borders of the screen and the node still needs updating).
*/
FCursorReply SDialogueViewportWidget::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (isPanning)
	{
		return FCursorReply::Cursor(EMouseCursor::None);
	}
	if (bBreakingLinksMode)
	{
		return FCursorReply::Cursor(EMouseCursor::Crosshairs);
	}

	// If we're dragging a node or multiple nodes.
	if (draggedNodeIndex != -1)
	{
		// All selected nodes besides the one under the cursor:
		for (auto NodeId : SelectedNodes)
		{
			int32 NodeIndex = NodeIdsIndexes.FindRef(NodeId);
			if (draggedNodeIndex != NodeIndex)
			{
				// NodeCoord = (MouseCoords + DragOffset) * Zoom - PanOffset + (difference in distance between this node and the node being dragged by the cursor)
				Dialogue->Data[NodeIndex].Coordinates = (MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition()) + draggingOffset) / GetZoomAmount() - panningOffset + (Dialogue->Data[NodeIndex].Coordinates - Dialogue->Data[draggedNodeIndex].Coordinates);
			}
		}
		// The one that is being dragged by the cursor:
		// MouseCoords = (NodeCoords + panningOffset) * Zoom - draggingOffset
		// <=> NodeCoords = (MouseCoords + draggingOffset) / Zoom - PanningOffset
		Dialogue->Data[draggedNodeIndex].Coordinates = (MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition()) + draggingOffset) / GetZoomAmount() - panningOffset;

		return FCursorReply::Cursor(EMouseCursor::CardinalCross);
	}

	if (isSelectingMultiple)
	{
		// sets marquee coordinates
		MarqueePointOfOrigin = FVector2D(
			(clickDownCoords.X + selectionPanningOffset.X) < dragMouseCoords.X ? clickDownCoords.X + selectionPanningOffset.X : dragMouseCoords.X,
			(clickDownCoords.Y + selectionPanningOffset.Y) < dragMouseCoords.Y ? clickDownCoords.Y + selectionPanningOffset.Y : dragMouseCoords.Y
			);

		MarqueeSize = FVector2D(
			(clickDownCoords.X + selectionPanningOffset.X) < dragMouseCoords.X ?
			(dragMouseCoords.X - clickDownCoords.X) - selectionPanningOffset.X :
			clickDownCoords.X - dragMouseCoords.X + selectionPanningOffset.X
			,
			(clickDownCoords.Y + selectionPanningOffset.Y) < dragMouseCoords.Y ?
			(dragMouseCoords.Y - clickDownCoords.Y) - selectionPanningOffset.Y :
			clickDownCoords.Y - dragMouseCoords.Y + selectionPanningOffset.Y
			);

		MarqueeEndPoint = MarqueePointOfOrigin + MarqueeSize;

		const FSlateRect MarqueeSlateRect(MarqueePointOfOrigin, MarqueeEndPoint);

		TArray<int32> temp_selectedNodeIds;
		TArray<int32> temp_difference = m_OriginallySelectedNodes;

		// finds nodes affected by marquee selection
		int nodeIndex = 0;
		for (auto Node : Dialogue->Data)
		{
			FVector2D NodeTopLeftPoint = (Node.Coordinates + panningOffset) * GetZoomAmount() - (NodeWidgets[nodeIndex]->NodeSize / 2);
			FVector2D NodeBottomRightPoint = NodeTopLeftPoint + NodeWidgets[nodeIndex]->NodeSize;
			FSlateRect NodeRect = FSlateRect(NodeTopLeftPoint, NodeBottomRightPoint);

			if (FSlateRect::DoRectanglesIntersect(MarqueeSlateRect, NodeRect))
			{
				temp_selectedNodeIds.Add(Node.id);
			}
			nodeIndex++;
		}

		// if we're selecting with Shift, we add nodes to current selection. If with Ctrl, we remove them from selection.
		if (CurrentSelection == ESelectionType::Type::WithShift)
		{
			for (auto originallySelectedNode : m_OriginallySelectedNodes)
			{
				temp_selectedNodeIds.AddUnique(originallySelectedNode);
			}
		}
		else if (CurrentSelection == ESelectionType::Type::WithCtrl)
		{
			for (auto curSelectedNode : temp_selectedNodeIds)
			{
				temp_difference.Remove(curSelectedNode);
			}
			temp_selectedNodeIds = temp_difference;
		}

		SelectNodes(temp_selectedNodeIds);
	}

	return FCursorReply::Unhandled();
}

FDialogueNode SDialogueViewportWidget::GetNodeById(int32 Id)
{
	return Dialogue->Data[NodeIdsIndexes.FindRef(Id)];
}

void SDialogueViewportWidget::StartDraggingIndex(int32 NodeIndex)
{
	// if we're currently dragging and we don't have any nodes selected, select this one and keep dragging it
	// if we're dragging, but have nodes selected:
	// if our current node is part of the selected nodes, drag all nodes, don't change selection
	// if not, then lose that old selection, select our current node and drag only it.
	if (!SelectedNodes.IsValidIndex(0))
	{
		SelectNodes(Dialogue->Data[NodeIndex].id);
	}
	else
	{
		if (!SelectedNodes.Contains(Dialogue->Data[NodeIndex].id))
		{
			DeselectAllNodes();
			SelectNodes(Dialogue->Data[NodeIndex].id);
			ForceRefresh();
		}
	}
	draggedNodeIndex = NodeIndex;
	ForceSlateToStayAwake();
}

void SDialogueViewportWidget::OnIsPlayerCommited(ECheckBoxState NewState)
{
	const FScopedTransaction Transaction(LOCTEXT("EditPcNpc", "Edited PC/NPC"));
	Dialogue->Modify();

	int32 index = NodeIdsIndexes.FindRef(EditingState->CurrentNodeId);
	Dialogue->Data[index].isPlayer = (NewState == ECheckBoxState::Checked);
}

ECheckBoxState SDialogueViewportWidget::GetIsPlayer() const
{
	if (EditingState->CurrentNodeId != -1)
	{
		int32 index = NodeIdsIndexes.FindRef(EditingState->CurrentNodeId);
		if (Dialogue->Data[index].isPlayer) return ECheckBoxState::Checked;
	}

	return ECheckBoxState::Unchecked;
}

void SDialogueViewportWidget::OnNodeTextCommited(const FText &InText, ETextCommit::Type)
{
	const FScopedTransaction Transaction(LOCTEXT("EditNodeText", "Edit Node Text"));
	Dialogue->Modify();

	int32 index = NodeIdsIndexes.FindRef(EditingState->CurrentNodeId);
	Dialogue->Data[index].Text = InText;
}


FText SDialogueViewportWidget::GetNodeText() const
{
	if (EditingState->CurrentNodeId != -1)
	{
		int32 index = EditingState->CurrentNodeId;
		return Dialogue->Data[index].Text;
	}

	return FText::GetEmpty();
}

FSlateFontInfo SDialogueViewportWidget::GetNodeFont()
{
	return NodeFont;
}

float SDialogueViewportWidget::GetNodeTextWrapLength()
{
	return nodeTextWrapLength;
}

FVector2D SDialogueViewportWidget::GetNodeMinSize()
{
	return nodeMinSize;
}

void SDialogueViewportWidget::UpdateFontInfo()
{
	NodeFont = FCoreStyle::GetDefaultFontStyle("Regular", fontSize);
	// NodeFont = FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), fontSize);
}

FMargin SDialogueViewportWidget::GetPos(int32 index)
{
	FMargin Output;

	Output.Left = (Dialogue->Data[index].Coordinates.X + panningOffset.X) * GetZoomAmount();
	Output.Top = (Dialogue->Data[index].Coordinates.Y + panningOffset.Y) * GetZoomAmount();

	return Output;
}

FReply SDialogueViewportWidget::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (isSelectingMultiple)
	{
		dragMouseCoords = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	}

	FReply Reply = FReply::Handled();

	if (isPanning)
	{
		panningOffset += MouseEvent.GetCursorDelta() / GetZoomAmount();

		// Capture the mouse if we haven't done it yet
		if (this->HasMouseCapture() == false)
		{
			Reply.CaptureMouse(AsShared()).UseHighPrecisionMouseMovement(AsShared());
			SoftwareCursorPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
			bShowSoftwareCursor = true;
		}

		SoftwareCursorPosition += MouseEvent.GetCursorDelta();
	}

	if (draggedNodeIndex != -1)
	{
		if (this->HasMouseCapture() == false)
		{
			Reply.CaptureMouse(AsShared());
		}

		FVector2D currentMouseCoords = MouseEvent.GetScreenSpacePosition();
		FSlateRect PanelScreenSpaceRect = MyGeometry.GetLayoutBoundingRect();

		UpdateAutoPanningDirection(currentMouseCoords, PanelScreenSpaceRect);

		FIntPoint BestPositionInPanel(
			FMath::RoundToInt(FMath::Clamp(currentMouseCoords.X, PanelScreenSpaceRect.Left, PanelScreenSpaceRect.Right)),
			FMath::RoundToInt(FMath::Clamp(currentMouseCoords.Y, PanelScreenSpaceRect.Top, PanelScreenSpaceRect.Bottom))
			);

		Reply.SetMousePos(BestPositionInPanel);
	}

	if (isSelectingMultiple)
	{
		if (this->HasMouseCapture() == false)
		{
			Reply.CaptureMouse(AsShared());
		}

		FVector2D currentMouseCoords = MouseEvent.GetScreenSpacePosition();
		FSlateRect PanelScreenSpaceRect = MyGeometry.GetLayoutBoundingRect();

		UpdateAutoPanningDirection(currentMouseCoords, PanelScreenSpaceRect);

		FIntPoint BestPositionInPanel(
			FMath::RoundToInt(FMath::Clamp(currentMouseCoords.X, PanelScreenSpaceRect.Left, PanelScreenSpaceRect.Right)),
			FMath::RoundToInt(FMath::Clamp(currentMouseCoords.Y, PanelScreenSpaceRect.Top, PanelScreenSpaceRect.Bottom))
			);

		Reply.SetMousePos(BestPositionInPanel);
	}

	if (EditingState->isLinking)
	{
		// We're still linking, but no node under the cursor. Cancelling linking.
		if (!isLinkingAndCapturing)
		{
			EditingState->isLinking = false;
			return Reply;
		}

		if (isLinkingAndCapturing && this->HasMouseCapture() == false)
		{
			Reply.CaptureMouse(AsShared());
		}

		EditingState->LinkingCoords = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		FVector2D currentMouseCoords = MouseEvent.GetScreenSpacePosition();
		FSlateRect PanelScreenSpaceRect = MyGeometry.GetLayoutBoundingRect();

		UpdateAutoPanningDirection(currentMouseCoords, PanelScreenSpaceRect);

		FIntPoint BestPositionInPanel(
			FMath::RoundToInt(FMath::Clamp(currentMouseCoords.X, PanelScreenSpaceRect.Left, PanelScreenSpaceRect.Right)),
			FMath::RoundToInt(FMath::Clamp(currentMouseCoords.Y, PanelScreenSpaceRect.Top, PanelScreenSpaceRect.Bottom))
			);

		Reply.SetMousePos(BestPositionInPanel);
	}

	return Reply;
}

FReply SDialogueViewportWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FReply Reply = FReply::Handled();

	dragMouseCoords = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	clickDownCoords = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	selectionPanningOffset = FVector2D(0, 0);

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Reply.DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
	}
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		Reply.DetectDrag(SharedThis(this), EKeys::RightMouseButton);
	}

	return Reply;
}

FReply SDialogueViewportWidget::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	EditTextNode = -1;

	clickUpCoords = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	FReply Reply = FReply::Unhandled();

	if (EditingState->isLinking)
	{
		if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton) // in case of RMB, we also cancel linking
		{
			EditingState->isLinking = false;
		}

		// cancel capture, which will result in either of two scenarios
		// 1) NodeWidget will get entered and will result in an attempt to link nodes
		// 2) or DialogueViewportWidget::OnMouseMove will cancel linking, seeing as no node was under the cursor
		// Potential problem: OnMouseMove will not trigger until the mouse returns to the screen. Is it a problem? Test it.
		if (isLinkingAndCapturing && this->HasMouseCapture())
		{
			isLinkingAndCapturing = false;
			SelectUnderCursor();
			return FReply::Handled().ReleaseMouseCapture(); // an alternative is FSlateApplication::Get().ReleaseMouseCapture();
		}
	}

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (flagForRefresh)
			ForceRefresh();

		if (bBreakingLinksMode)
		{
			bBreakingLinksMode = false;
			return Reply;
		}

		if (draggedNodeIndex != -1)
		{
			// TODO: this may result in quite a resort due to duplicates. We can drag 5 children of the same parent, and the result is that we'll resort the parent 5 times. Write a method optimizing it in the future.
			for (auto nodeId : SelectedNodes)
			{
				int32 nodeIndex = NodeIdsIndexes.FindRef(nodeId);
				NodeWidgets[nodeIndex]->SortParentsLinks();
			}
			draggedNodeIndex = -1;
			return FReply::Handled().ReleaseMouseCapture();
		}

		if (isSelectingMultiple)
		{
			isSelectingMultiple = false;
			return FReply::Handled().ReleaseMouseCapture();
		}
		else
		{
			if (MouseEvent.IsShiftDown())
			{
				CurrentSelection = ESelectionType::Type::WithShift;
			}
			else if (MouseEvent.IsControlDown())
			{
				CurrentSelection = ESelectionType::Type::WithCtrl;
			}
			else
			{
				CurrentSelection = ESelectionType::Type::Default;
			}
			SelectUnderCursor();
		}
	}
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (isPanning)
		{
			isPanning = false;
			Reply = FReply::Handled().ReleaseMouseCapture();

			bShowSoftwareCursor = false;

			FSlateRect PanelScreenSpaceRect = MyGeometry.GetLayoutBoundingRect();
			FVector2D CursorPosition = MyGeometry.LocalToAbsolute(SoftwareCursorPosition);

			FIntPoint BestPositionInPanel(
				FMath::RoundToInt(FMath::Clamp(CursorPosition.X, PanelScreenSpaceRect.Left, PanelScreenSpaceRect.Right)),
				FMath::RoundToInt(FMath::Clamp(CursorPosition.Y, PanelScreenSpaceRect.Top, PanelScreenSpaceRect.Bottom))
				);

			Reply.SetMousePos(BestPositionInPanel);
		}
		else if (bBreakingLinksMode)
		{
			bBreakingLinksMode = false;
			return Reply;
		}

		if (FVector2D::Distance(clickUpCoords, clickDownCoords) > 2)
		{
			return Reply;
		}

		// If dragging a node and right clicked, do nothing.
		if (draggedNodeIndex != -1)
		{
			return Reply;
		}

		// Right click menu when clicked on the background.
		{
			FMenuBuilder MenuBuilder(true, NULL);

			FUIAction AddNodeAction(FExecuteAction::CreateSP(this, &SDialogueViewportWidget::OnAddNodeClicked));

			MenuBuilder.BeginSection(NAME_None, NSLOCTEXT("PropertyView", "ExpansionHeading", "Dialogue"));
			MenuBuilder.AddMenuEntry(NSLOCTEXT("PropertyView", "AddNode", "Add Node"), NSLOCTEXT("PropertyView", "AddNode_ToolTip", "Adds an empty node"), FSlateIcon(), AddNodeAction);
			MenuBuilder.EndSection();

			FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();

			FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, MenuBuilder.MakeWidget(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect::ContextMenu);
		}
	}
	return Reply;
}

FReply SDialogueViewportWidget::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// we're panning
	if (MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton) && !EditingState->isLinking && !isSelectingMultiple && draggedNodeIndex == -1)
	{
		isPanning = true;
	}
	// we're selecting multiple
	else if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && !isPanning && !EditingState->isLinking && draggedNodeIndex == -1 && !bBreakingLinksMode)
	{
		m_OriginallySelectedNodes = SelectedNodes;

		if (MouseEvent.IsShiftDown())
		{
			CurrentSelection = ESelectionType::Type::WithShift;
		}
		else if (MouseEvent.IsControlDown())
		{
			CurrentSelection = ESelectionType::Type::WithCtrl;
		}
		else
		{
			CurrentSelection = ESelectionType::Type::Default;
		}
		isSelectingMultiple = true;
		ForceSlateToStayAwake();
	}

	return FReply::Unhandled();
}

/* Updates the direction and intensity for panning, depending on mouse cursor's distance from the borders.
* The smaller the distance, the faster the panning.
*/
void SDialogueViewportWidget::UpdateAutoPanningDirection(FVector2D& currentMouseCoords, FSlateRect& PanelScreenSpaceRect)
{
	if (currentMouseCoords.X >= PanelScreenSpaceRect.Right - 40)
	{
		float clampedX = currentMouseCoords.X;

		if (clampedX > PanelScreenSpaceRect.Right) clampedX = PanelScreenSpaceRect.Right;

		float xFromBorder = 40 - (PanelScreenSpaceRect.Right - clampedX);

		AutoPanningDirection.X = -xFromBorder;
	}
	else if (currentMouseCoords.X <= PanelScreenSpaceRect.Left + 40)
	{
		float clampedX = currentMouseCoords.X;

		if (clampedX < PanelScreenSpaceRect.Left) clampedX = PanelScreenSpaceRect.Left;

		float xFromBorder = 40 + (PanelScreenSpaceRect.Left - clampedX);

		AutoPanningDirection.X = xFromBorder;
	}
	else
	{
		AutoPanningDirection.X = 0.0f;
	}

	if (currentMouseCoords.Y >= PanelScreenSpaceRect.Bottom - 40)
	{
		float clampedY = currentMouseCoords.Y;

		if (clampedY > PanelScreenSpaceRect.Bottom) clampedY = PanelScreenSpaceRect.Bottom;

		float yFromBorder = 40 - (PanelScreenSpaceRect.Bottom - clampedY);

		AutoPanningDirection.Y = -yFromBorder;
	}
	else if (currentMouseCoords.Y <= PanelScreenSpaceRect.Top + 40)
	{
		float clampedY = currentMouseCoords.Y;

		if (clampedY < PanelScreenSpaceRect.Top) clampedY = PanelScreenSpaceRect.Top;

		float yFromBorder = 40 + (PanelScreenSpaceRect.Top - clampedY);

		AutoPanningDirection.Y = yFromBorder;
	}
	else
	{
		AutoPanningDirection.Y = 0.0f;
	}
}

void SDialogueViewportWidget::OnAddNodeClicked()
{
	const FScopedTransaction Transaction(LOCTEXT("AddNode", "Node Added"));
	Dialogue->Modify();

	FDialogueNode NewNode;

	NewNode.id = Dialogue->NextNodeId;
	// mouseCoords = (nodeCoords + panning) * Zoom
	// <=> mouseCoords/Zoom = nodeCoords + panning
	// <=> nodeCoords = mouseCoords/Zoom - panning
	NewNode.Coordinates = clickUpCoords / GetZoomAmount() - panningOffset;
	NewNode.Text = FText::FromString(TEXT("Enter some text"));
	Dialogue->NextNodeId++;
	Dialogue->Data.Add(NewNode);
	EditingState->CurrentNodeId = NewNode.id; //select the new node
	SpawnNodes(NewNode.id);
}

int32 SDialogueViewportWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 maxLayerId = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FSlateBrush* BackgroundImage = FAppStyle::GetBrush(TEXT("Graph.Panel.SolidBackground"));
	PaintBackgroundAsLines(BackgroundImage, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId);

	LayerId -= 4; // @TODO: experiment with this value... set it to -=2, -=1 etc to see how it goes.

	// Draw software cursor
	if (bShowSoftwareCursor)
	{
		const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("SoftwareCursor_Grab"));
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			maxLayerId, // draws the mouse cursor on top of all nodes
			AllottedGeometry.ToPaintGeometry(Brush->ImageSize, FSlateLayoutTransform(SoftwareCursorPosition - (Brush->ImageSize / 2))),
			Brush
			);
	}

	// Display a marquee box when selecting multiple nodes
	if (isSelectingMultiple)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId+3,
			AllottedGeometry.ToPaintGeometry(MarqueeSize, FSlateLayoutTransform(MarqueePointOfOrigin)),
			FAppStyle::GetBrush("FocusRectangle"), // or MarqueeSelection ?
			ESlateDrawEffect::None,
			FColor(255, 255, 255, 255) //FLinearColor::Green *.65f
			);
	}

	// if we're in Play mode, display a yellow border inside the window
	if (ShowGraphStateOverlay.Get())
	{
		const FSlateBrush* BorderBrush = nullptr;
		if ((GEditor->bIsSimulatingInEditor || GEditor->PlayWorld != nullptr))
		{
			// Draw a surrounding indicator when PIE is active, to make it clear that the graph is read-only, etc...
			BorderBrush = FAppStyle::GetBrush(TEXT("Graph.PlayInEditor"));
		}
		else if (!IsEditable.Get())
		{
			// Draw a different border when we're not simulating but the graph is read-only
			BorderBrush = FAppStyle::GetBrush(TEXT("Graph.ReadOnlyBorder"));
		}

		if (BorderBrush != nullptr)
		{
			// Actually draw the border
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				maxLayerId,
				AllottedGeometry.ToPaintGeometry(),
				BorderBrush
				);
		}
	}

	// draw a shadow overlay around the edges of the screen
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			maxLayerId,
			AllottedGeometry.ToPaintGeometry(),
			FAppStyle::GetBrush(TEXT("Graph.Shadow"))
			);
	}

	int nodeIndex = 0;

	// draw lines and arrows between nodes, representing links
	for (auto Node : Dialogue->Data) //for each dialogue node
	{
		// Draw node shadow/selection overlay for this node
		if (isNodeVisible[nodeIndex])
		{
			FVector2D overlaySize = NodeWidgets[nodeIndex]->NodeSize + FVector2D(24.f, 24.f);
			FVector2D upperLeft = (Node.Coordinates + panningOffset) * GetZoomAmount() - (overlaySize / 2.f);

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 5, // @TODO: why +5? Try different values?
				AllottedGeometry.ToPaintGeometry(FVector2D(overlaySize), FSlateLayoutTransform(FVector2D(upperLeft))),
				FAppStyle::GetBrush(NodeWidgets[nodeIndex]->isSelected ? TEXT("Graph.Node.ShadowSelected")  : TEXT("Graph.Node.Shadow"))
				);
		}


		for (auto Link : Node.Links) // for each link
		{
			int32 linkIndex = NodeIdsIndexes.FindRef(Link);
			if (linkIndex != -1)
			{
				// If the child node is lower than its parent, we draw a line to it. Otherwise we draw a spline

				// Splines:
				if (Node.Coordinates.Y > Dialogue->Data[linkIndex].Coordinates.Y)
				{
					FVector2D fromNodeSize = NodeWidgets[nodeIndex]->NodeSize;
					FVector2D startPoint(
						(Node.Coordinates.X + panningOffset.X - 8) * GetZoomAmount() + (fromNodeSize.X / 2),
						(Node.Coordinates.Y + panningOffset.Y) * GetZoomAmount()
					);

					FVector2D toNodeSize = NodeWidgets[linkIndex]->NodeSize;
					FVector2D endPoint(
						(Dialogue->Data[linkIndex].Coordinates.X + panningOffset.X + 3 ) * GetZoomAmount() - (toNodeSize.X / 2),
						(Dialogue->Data[linkIndex].Coordinates.Y + panningOffset.Y) * GetZoomAmount()
					);

					// 1. if only one node is selected and if this node is the one selected, then draw blue lines (outgoing)
					if (SelectedNodes.Num() == 1 && SelectedNodes[0] == Node.id)
					{
						FSlateDrawElement::MakeSpline(
							OutDrawElements,
							LayerId + 3,
							AllottedGeometry.ToPaintGeometry(),
							startPoint, ComputeSplineTangent(startPoint, endPoint),
							endPoint, ComputeSplineTangent(startPoint, endPoint),
							2.0f,
							ESlateDrawEffect::None,
							FLinearColor(0.013575f, 0.770000f, 0.429609f, 1.0f) //green-blue
							);
					}
					// 2. else if only one node is selected if this is the node that's being linked, draw red lines (incoming)
					else if(SelectedNodes.Num() == 1 && SelectedNodes[0] == Link)
					{
						FSlateDrawElement::MakeSpline(
							OutDrawElements,
							LayerId + 3,
							AllottedGeometry.ToPaintGeometry(),
							startPoint, ComputeSplineTangent(startPoint, endPoint),
							endPoint, ComputeSplineTangent(startPoint, endPoint),
							2.0f,
							ESlateDrawEffect::None,
							FLinearColor(0.607717f, 0.224984f, 1.0f, 1.0f) // lilac
							);
					}
					// 3. else, for all other lines, draw gray lines if DisplayIdleSplies is checked
					else if (Dialogue->DisplayIdleSplines && ZoomLevel > 6)
					{
						FSlateDrawElement::MakeSpline(
							OutDrawElements,
							LayerId + 3,
							AllottedGeometry.ToPaintGeometry(),
							startPoint, ComputeSplineTangent(startPoint, endPoint),
							endPoint, ComputeSplineTangent(startPoint, endPoint),
							2.0f,
							ESlateDrawEffect::None,
							FLinearColor(0.220000f, 0.195800f, 0.195800f, 0.2f) // grey
							);
					}
				}
				// Lines:
				else
				{
					TArray<FVector2D> LinePoints;

					// FROM
					FVector2D fromNodeSize = NodeWidgets[nodeIndex]->NodeSize;
					LinePoints.Add(FVector2D(
						(Node.Coordinates.X + panningOffset.X) * GetZoomAmount(),
						(Node.Coordinates.Y + panningOffset.Y - 3) * GetZoomAmount() + fromNodeSize.Y / 2
					));

					// TO
					FVector2D toNodeSize = NodeWidgets[linkIndex]->NodeSize;
					LinePoints.Add(FVector2D(
						(Dialogue->Data[linkIndex].Coordinates.X + panningOffset.X) * GetZoomAmount(),
						(Dialogue->Data[linkIndex].Coordinates.Y + panningOffset.Y) * GetZoomAmount() - toNodeSize.Y / 2
					));

					FSlateDrawElement::MakeLines(
						OutDrawElements,
						LayerId + 4,
						AllottedGeometry.ToPaintGeometry(),
						LinePoints,
						ESlateDrawEffect::None,
						FLinearColor::White
						);

					// Draw an arrow in the middle of the line:
					if (ZoomLevel > 6) // only happens at -5 or higher
					{
						FVector2D dir = (LinePoints[1] - LinePoints[0]);
						dir.Normalize();
						FVector2D middleOfLine = LinePoints[0] + ((LinePoints[1] - LinePoints[0]) * 0.5) + dir * 5;

						TArray<FVector2D> ArrowPoints;

						FVector2D arrowPoint = LinePoints[0] - LinePoints[1];
						arrowPoint = arrowPoint.GetRotated(20);
						arrowPoint.Normalize();
						arrowPoint = middleOfLine + arrowPoint * 12 * GetZoomAmount();
						ArrowPoints.Add(arrowPoint);

						ArrowPoints.Add(middleOfLine);

						TArray<FVector2D> ArrowPoints2;

						ArrowPoints2.Add(middleOfLine);

						arrowPoint = LinePoints[0] - LinePoints[1];
						arrowPoint = arrowPoint.GetRotated(-20);
						arrowPoint.Normalize();
						arrowPoint = middleOfLine + arrowPoint * 12 * GetZoomAmount();
						ArrowPoints2.Add(arrowPoint);

						FSlateDrawElement::MakeLines(
							OutDrawElements,
							LayerId + 4,
							AllottedGeometry.ToPaintGeometry(),
							ArrowPoints,
							ESlateDrawEffect::None,
							FLinearColor::White
						);

						FSlateDrawElement::MakeLines(
							OutDrawElements,
							LayerId + 4,
							AllottedGeometry.ToPaintGeometry(),
							ArrowPoints2,
							ESlateDrawEffect::None,
							FLinearColor::White
						);
					}
				}
			}
		}
		nodeIndex++;
	}

	// draw linking line if we're in linking mode
	if (EditingState->isLinking)
	{
		TArray<FVector2D> LinkingPoints;

		FVector2D fromNodeSize = NodeWidgets[EditingState->LinkingFromIndex]->NodeSize;
		LinkingPoints.Add(FVector2D(
			(Dialogue->Data[EditingState->LinkingFromIndex].Coordinates.X + panningOffset.X) * GetZoomAmount(),
			(Dialogue->Data[EditingState->LinkingFromIndex].Coordinates.Y + panningOffset.Y - 3) * GetZoomAmount() + fromNodeSize.Y / 2
		));
		LinkingPoints.Add(EditingState->LinkingCoords);

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId + 4,
			AllottedGeometry.ToPaintGeometry(),
			LinkingPoints,
			ESlateDrawEffect::None,
			FLinearColor::White
			);
	}

	return maxLayerId;
}

float SDialogueViewportWidget::GetZoomAmount() const
{
	return ZoomLevels->GetZoomAmount(ZoomLevel);
}

FText SDialogueViewportWidget::GetZoomText() const
{
	return ZoomLevels->GetZoomText(ZoomLevel);
}

FSlateColor SDialogueViewportWidget::GetZoomTextColorAndOpacity() const
{
	return FLinearColor(1, 1, 1, 1.25f - ZoomLevelFade.GetLerp());
}

inline float FancyMod(float Value, float Size)
{
	return ((Value >= 0) ? 0.0f : Size) + FMath::Fmod(Value, Size);
}

FVector2D SDialogueViewportWidget::GetViewOffset() const
{
	return -panningOffset;
}

int32 SDialogueViewportWidget::GetZoomLevel() const
{
	return ZoomLevel;
}

FVector2D SDialogueViewportWidget::GraphCoordToPanelCoord(const FVector2D& GraphSpaceCoordinate) const
{
	return (GraphSpaceCoordinate - GetViewOffset()) * GetZoomAmount();
}

FReply SDialogueViewportWidget::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// We want to zoom into this point; i.e. keep it the same fraction offset into the panel
	const FVector2D WidgetSpaceCursorPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const int32 ZoomLevelDelta = FMath::FloorToInt(MouseEvent.GetWheelDelta());
	ChangeZoomLevel(ZoomLevelDelta, WidgetSpaceCursorPos, MouseEvent.IsControlDown());

	return FReply::Handled();
}

void SDialogueViewportWidget::ChangeZoomLevel(int32 ZoomLevelDelta, const FVector2D& WidgetSpaceZoomOrigin, bool bOverrideZoomLimiting)
{
	// WidgetSpaceZoomOrigin = Local coords in the widget

	// We want to zoom into this point; i.e. keep it the same fraction offset into the panel
	const FVector2D PointToMaintainGraphSpace = WidgetSpaceZoomOrigin / GetZoomAmount() + GetViewOffset();

	const int32 DefaultZoomLevel = ZoomLevels->GetDefaultZoomLevel(); // 12
	const int32 NumZoomLevels = ZoomLevels->GetNumZoomLevels(); // 20

	const bool bAllowFullZoomRange =
		// To zoom in past 1:1 the user must press control
		(ZoomLevel == DefaultZoomLevel && ZoomLevelDelta > 0 && bOverrideZoomLimiting) ||
		// If they are already zoomed in past 1:1, user may zoom freely
		(ZoomLevel > DefaultZoomLevel);

	const float OldZoomLevel = ZoomLevel;

	if (bAllowFullZoomRange)
	{
		ZoomLevel = FMath::Clamp(ZoomLevel + ZoomLevelDelta, 0, NumZoomLevels - 1);
	}
	else
	{
		// Without control, we do not allow zooming in past 1:1.
		ZoomLevel = FMath::Clamp(ZoomLevel + ZoomLevelDelta, 0, DefaultZoomLevel);
	}

	if (OldZoomLevel != ZoomLevel)
	{
		PostChangedZoom();
		UpdateZoomAffectedValues();
	}

	// Note: This happens even when maxed out at a stop; so the user sees the animation and knows that they're at max zoom in/out
	ZoomLevelFade.Play(this->AsShared());

	// Re-center the screen so that it feels like zooming around the cursor.
	panningOffset = - (PointToMaintainGraphSpace - WidgetSpaceZoomOrigin / GetZoomAmount());

	//UE_LOG(LogTemp, Log, TEXT("Zoom: %f"), GetZoomAmount());

	shouldTickAllWidgets = true;
}

EVisibility SDialogueViewportWidget::IsSimulating() const
{
	if (ShowGraphStateOverlay.Get() && (GEditor->bIsSimulatingInEditor || GEditor->PlayWorld != NULL))
	{
		return EVisibility::Visible;
	}
	return EVisibility::Hidden;
}

void SDialogueViewportWidget::UpdateZoomAffectedValues()
{
	switch (ZoomLevel)
	{
		// zoom -12
		case 0:
		{

			break;
		}

		// zoom -11
		case 1:
		{

			break;
		}

		// zoom -10
		case 2:
		{

			break;
		}

		// zoom -9
		case 3:
		{

			break;
		}

		// zoom -8
		case 4:
		{
			// all nodes become the same size here (the text is collapsed)
			break;
		}

		// zoom -7
		case 5:
		{
			// the text is collapsed
			// all nodes become the same size
			break;
		}

		// zoom -6
		case 6:
		{
			// no visible text at this point
			// but the text still dictates the size of the node (hidden, but not collapsed?)
			// although with 1 font size it doesn't dictate anything really...
			fontSize = 1;
			nodeTextWrapLength = 30;
			//fontSize = 2;
			//nodeTextWrapLength = 50;
			break;
		}

		// zoom -5
		case 7:
		{
			fontSize = 3; // should be 3.5
			nodeTextWrapLength = 80;
			break;
		}

		// zoom -4
		case 8:
		{
			fontSize = 4; // should be 4.5
			nodeTextWrapLength = 110;
			break;
		}

		// zoom -3
		case 9:
		{
			fontSize = 6; // should be 5.5
			nodeTextWrapLength = 170;
			break;
		}

		// zoom -2
		case 10:
		{
			fontSize = 7;
			nodeTextWrapLength = 183;
			break;
		}

		// zoom -1
		case 11:
		{
			fontSize = 8;
			nodeTextWrapLength = 230;
			break;
		}

		// zoom 1:1
		case 12:
		{
			fontSize = 9;
			nodeTextWrapLength = 250;
			break;
		}

		// zoom +1
		case 13:
		{
			fontSize = 11;
			nodeTextWrapLength = 325;
			break;
		}

		// zoom +2
		case 14:
		{
			fontSize = 13;
			nodeTextWrapLength = 360;
			break;
		}

		// zoom +3
		case 15:
		{
			fontSize = 14;
			nodeTextWrapLength = 405;
			break;
		}

		// zoom +4
		case 16:
		{
			fontSize = 16;
			nodeTextWrapLength = 440;
			break;
		}

		// zoom +5
		case 17:
		{
			fontSize = 17;
			nodeTextWrapLength = 480;
			break;
		}

		// zoom +6
		case 18:
		{
			fontSize = 18;
			nodeTextWrapLength = 520;
			break;
		}

		// zoom +7
		case 19:
		{
			fontSize = 19;
			nodeTextWrapLength = 530;
			break;
		}
	}
	UpdateFontInfo();

	nodeMinSize = FVector2D(144, 60) * GetZoomAmount();
}

void SDialogueViewportWidget::PostChangedZoom()
{
	CurrentLOD = ZoomLevels->GetLOD(ZoomLevel);
}

TOptional<FSlateRenderTransform> SDialogueViewportWidget::GetIconScale()
{
	return TOptional<FSlateRenderTransform>(FSlateRenderTransform(GetZoomAmount()));
}

FMargin SDialogueViewportWidget::GetLeftCornerPadding()
{
	return FMargin(5 * GetZoomAmount(), 0, 0, 0);
}

/**
* same as SNodePanel::PaintBackgroundAsLines
*/
void SDialogueViewportWidget::PaintBackgroundAsLines(const FSlateBrush* BackgroundImage, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32& DrawLayerId) const
{
	const bool bAntialias = false;
	const int32 RulePeriod = 8; // = (int32)FAppStyle::GetFloat("Graph.Panel.GridRulePeriod");
	check(RulePeriod > 0);

	const FLinearColor RegularColor(FAppStyle::GetColor("Graph.Panel.GridLineColor"));
	const FLinearColor RuleColor(FAppStyle::GetColor("Graph.Panel.GridRuleColor"));
	const FLinearColor CenterColor(FAppStyle::GetColor("Graph.Panel.GridCenterColor"));
	const float GraphSmallestGridSize = 8.0f;
	const float RawZoomFactor = GetZoomAmount();
	const float NominalGridSize = GetSnapGridSize();

	float ZoomFactor = RawZoomFactor;
	float Inflation = 1.0f;
	while (ZoomFactor*Inflation*NominalGridSize <= GraphSmallestGridSize)
	{
		Inflation *= 2.0f;
	}

	const float GridCellSize = NominalGridSize * ZoomFactor * Inflation;

	const float GraphSpaceGridX0 = FancyMod(-panningOffset.X, Inflation * NominalGridSize * RulePeriod);
	const float GraphSpaceGridY0 = FancyMod(-panningOffset.Y, Inflation * NominalGridSize * RulePeriod);

	float ImageOffsetX = GraphSpaceGridX0 * -ZoomFactor;
	float ImageOffsetY = GraphSpaceGridY0 * -ZoomFactor;

	//float ImageOffsetX = GraphSpaceGridX0 -GetSnapGridSize() * GraphSmallestGridSize; //@TODO: multiply by zoom factor?
	//float ImageOffsetY = GraphSpaceGridY0 -GetSnapGridSize() * GraphSmallestGridSize;

	const FVector2D ZeroSpace = GraphCoordToPanelCoord(FVector2D::ZeroVector);

	// Fill the background
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		DrawLayerId,
		AllottedGeometry.ToPaintGeometry(),
		BackgroundImage
	);

	TArray<FVector2D> LinePoints;
	new (LinePoints) FVector2D(0.0f, 0.0f);
	new (LinePoints) FVector2D(0.0f, 0.0f);

	// Horizontal bars
	for (int32 GridIndex = 0; ImageOffsetY < AllottedGeometry.Size.Y; ImageOffsetY += GridCellSize, ++GridIndex)
	{
		if (ImageOffsetY >= 0.0f)
		{
			const bool bIsRuleLine = (GridIndex % RulePeriod) == 0;
			const int32 Layer = bIsRuleLine ? (DrawLayerId + 1) : DrawLayerId;

			const FLinearColor* Color = bIsRuleLine ? &RuleColor : &RegularColor;

			LinePoints[0] = FVector2D(0.0f, ImageOffsetY);
			LinePoints[1] = FVector2D(AllottedGeometry.Size.X, ImageOffsetY);

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				Layer,
				AllottedGeometry.ToPaintGeometry(),
				LinePoints,
				ESlateDrawEffect::None,
				*Color,
				bAntialias);
		}
	}

	// Vertical bars
	for (int32 GridIndex = 0; ImageOffsetX < AllottedGeometry.Size.X; ImageOffsetX += GridCellSize, ++GridIndex)
	{
		if (ImageOffsetX >= 0.0f)
		{
			const bool bIsRuleLine = (GridIndex % RulePeriod) == 0;
			const int32 Layer = bIsRuleLine ? (DrawLayerId + 1) : DrawLayerId;

			const FLinearColor* Color = bIsRuleLine ? &RuleColor : &RegularColor;

			LinePoints[0] = FVector2D(ImageOffsetX, 0.0f);
			LinePoints[1] = FVector2D(ImageOffsetX, AllottedGeometry.Size.Y);

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				Layer,
				AllottedGeometry.ToPaintGeometry(),
				LinePoints,
				ESlateDrawEffect::None,
				*Color,
				bAntialias);
		}
	}

	DrawLayerId += 2; // puts the overlay for the first node in front of the node itself
}

/**
* same as UGraphEditorSettings::ComputeSplineTangent
*/
FVector2D SDialogueViewportWidget::ComputeSplineTangent(const FVector2D& Start, const FVector2D& End) const
{
	const FVector2D DeltaPos = End - Start;
	const bool bGoingForward = DeltaPos.X >= 0.0f;

	const float ClampedTensionX = FMath::Min<float>(FMath::Abs<float>(DeltaPos.X), bGoingForward ? GetDefault<UGraphEditorSettings>()->ForwardSplineHorizontalDeltaRange : GetDefault<UGraphEditorSettings>()->BackwardSplineHorizontalDeltaRange);
	const float ClampedTensionY = FMath::Min<float>(FMath::Abs<float>(DeltaPos.Y), bGoingForward ? GetDefault<UGraphEditorSettings>()->ForwardSplineVerticalDeltaRange : GetDefault<UGraphEditorSettings>()->BackwardSplineVerticalDeltaRange);

	if (bGoingForward)
	{
		return (ClampedTensionX * GetDefault<UGraphEditorSettings>()->ForwardSplineTangentFromVerticalDelta) + (ClampedTensionY * GetDefault<UGraphEditorSettings>()->ForwardSplineTangentFromVerticalDelta);
	}
	else
	{
		return (ClampedTensionX * GetDefault<UGraphEditorSettings>()->BackwardSplineTangentFromHorizontalDelta) + (ClampedTensionY * GetDefault<UGraphEditorSettings>()->BackwardSplineTangentFromVerticalDelta);
	}
}

void SDialogueViewportWidget::SelectNodes(TArray<int32> NodesIds) const
{
	for (auto nodeId : SelectedNodes)
	{
		int32 nodeIndex = NodeIdsIndexes.FindRef(nodeId);
		if (nodeIndex != -1) // prevents issues for window draws while undoing/redoing
		{
			NodeWidgets[nodeIndex]->isSelected = false;
		}
	}
	SelectedNodes.Empty();

	SelectedNodes = NodesIds;

	for (auto nodeId : SelectedNodes)
	{
		int32 nodeIndex = NodeIdsIndexes.FindRef(nodeId);
		if (nodeIndex != -1) // prevents issues for window draws while undoing/redoing
		{
			NodeWidgets[nodeIndex]->isSelected = true;
		}

	}

	if (SelectedNodes.Num() > 1)
	{
		EditingState->CurrentNodeId = -1;
	}
	else if (SelectedNodes.Num() == 1)
	{
		EditingState->CurrentNodeId = SelectedNodes[0];
		flagForRefresh = true;
	}
	else if (SelectedNodes.Num() == 0)
	{
		EditingState->CurrentNodeId = -1;
	}
}

void SDialogueViewportWidget::ForceRefresh() const
{
	DialogueEditorPtr.Pin()->refreshDetails = true;
}

void SDialogueViewportWidget::SelectNodes(int32 NodeId) const
{
	TArray<int32> justOneNode;
	justOneNode.Add(NodeId);
	SelectNodes(justOneNode);
}

/*
* Resource intensive (because of ForceRefresh). Do not use in OnMouseMove and the likes.
* Checks for each node if the rect, formed by its coordinates and size, contains the mouse ClickUpCoords.
*/
void SDialogueViewportWidget::SelectUnderCursor() const
{
	int nodeIndex = 0;
	for (auto Node : Dialogue->Data)
	{
		// Determening node rect
		FVector2D NodeTopLeftPoint = (Node.Coordinates + panningOffset) * GetZoomAmount() - (NodeWidgets[nodeIndex]->NodeSize / 2);
		FVector2D NodeBottomRightPoint = (Node.Coordinates + panningOffset) * GetZoomAmount() + (NodeWidgets[nodeIndex]->NodeSize / 2);
		FSlateRect NodeRect = FSlateRect(NodeTopLeftPoint, NodeBottomRightPoint);

		if (NodeRect.ContainsPoint(clickUpCoords))
		{
			if ( (CurrentSelection == ESelectionType::Type::WithShift || CurrentSelection == ESelectionType::Type::WithCtrl) && !SelectedNodes.Contains(Node.id) )
			{
				SelectedNodes.Add(Node.id);
				NodeWidgets[nodeIndex]->isSelected = true;
			}
			else if (CurrentSelection == ESelectionType::Type::WithCtrl && SelectedNodes.Contains(Node.id))
			{
				SelectedNodes.Remove(Node.id);
				NodeWidgets[nodeIndex]->isSelected = false;
			}
			else if (CurrentSelection == ESelectionType::Type::Default)
			{
				SelectNodes(Node.id);
			}
			ForceRefresh();
			return;
		}
		nodeIndex++;
	}
	DeselectAllNodes();
}

void SDialogueViewportWidget::DeselectAllNodes() const
{
	SelectNodes(TArray<int32>());
}

void SDialogueViewportWidget::DeselectNode(int32 NodeId) const
{
	SelectedNodes.Remove(NodeId);
}

/** To understand what's going on here, read this post https://www.unrealengine.com/blog/utilizing-slate-sleep-and-active-timers
* The "sleeping ui" causes problems for "auto-panning" due to the cursor being close to the screen border,
* so we force Slate to stay awake while we're dragging a node, selecting multiple nodes or linking nodes. */
EActiveTimerReturnType SDialogueViewportWidget::EmptyTimer(double InCurrentTime, float InDeltaTime)
{
	if (EditingState->isLinking || draggedNodeIndex != -1 || isSelectingMultiple)
		return EActiveTimerReturnType::Continue;
	else
		return EActiveTimerReturnType::Stop;
}

void SDialogueViewportWidget::ForceSlateToStayAwake()
{
	SWidget::RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SDialogueViewportWidget::EmptyTimer));
}

FReply SDialogueViewportWidget::OnKeyDown(const FGeometry & MyGeometry, const FKeyEvent & InKeyEvent)
{
	if (DialogueEditorPtr.Pin()->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	else
	{
		if (InKeyEvent.GetKey() == EKeys::P)
		{
			//shouldTickAllWidgets = true;
			//UE_LOG(LogTemp, Log, TEXT("Attempting to tick all widgets."));
		}
		else if (InKeyEvent.GetKey() == EKeys::O)
		{
			//UE_LOG(LogTemp, Log, TEXT("Decreasing font size."));
			//SetFontSize(fontSize - 1);
		}
		return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}
}

void SDialogueViewportWidget::OnFocusLost(const FFocusEvent & InFocusEvent)
{
	/*UE_LOG(LogTemp, Log, TEXT("DialogueViewportWidget: Focus lost."));*/
}

FReply SDialogueViewportWidget::OnFocusReceived(const FGeometry & MyGeometry, const FFocusEvent & InFocusEvent)
{
	/*UE_LOG(LogTemp, Log, TEXT("DialogueViewportWidget: Focus received."));*/
	return FReply::Unhandled();
}
/*
* Is called when deleting a single node from node context menu.
*/
void SDialogueViewportWidget::DeleteOneNode(int32 nodeId)
{
	// we never delete the first node
	if (nodeId == 0)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("DeleteNode", "Delete Node"));
	Dialogue->Modify();

	int32 Index = NodeIdsIndexes.FindRef(nodeId);

	EditingState->CurrentNodeId = -1;
	NodeWidgets[Index]->OnDeleteNode(true); // it also calls SpawnNodes()
	ForceRefresh();
}

void SDialogueViewportWidget::DeleteSelected()
{
	// no nodes were selected
	if (!SelectedNodes.IsValidIndex(0))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("DeleteNodes", "Delete Nodes"));
	Dialogue->Modify();

	for (int i = SelectedNodes.Num() - 1; i >= 0; i--)
	{
		int32 Id = SelectedNodes[i];
		int32 Index = NodeIdsIndexes.FindRef(Id);

		// we never delete the first node
		if (Id == 0)
		{
			continue;
		}

		NodeWidgets[Index]->OnDeleteNode(false);
	}
	EditingState->CurrentNodeId = -1;
	SpawnNodes();
	ForceRefresh();
}

void SDialogueViewportWidget::DuplicateSelected()
{
	if (SelectedNodes.Num() > 0)
	{
		CopySelected();
		PasteNodes();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Log: No nodes selected to duplicate."));
	}
}

void SDialogueViewportWidget::CopySelected()
{
	FString Text;
	if (FDialogueGraphClipboard::ExportNodes(Dialogue, SelectedNodes, Text))
		FPlatformApplicationMisc::ClipboardCopy(*Text);
}

void SDialogueViewportWidget::CutSelected()
{
	if (SelectedNodes.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Log: Cutting %d nodes."), SelectedNodes.Num());
		CopySelected();
		DeleteSelected();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Log: No nodes selected to cut."));
	}
}

void SDialogueViewportWidget::PasteNodes()
{
	if (GEditor->PlayWorld) return;
	FString Text;
	FPlatformApplicationMisc::ClipboardPaste(Text);
	TArray<int32> IDs;
	if (FDialogueGraphClipboard::ImportNodes(Dialogue, Text, FVector2D(CoordsForPasting), IDs))
	{
		SpawnNodes(IDs.Num() == 1 ? IDs[0] : INDEX_NONE);
		SelectNodes(IDs);
	}
}

#undef LOCTEXT_NAMESPACE
