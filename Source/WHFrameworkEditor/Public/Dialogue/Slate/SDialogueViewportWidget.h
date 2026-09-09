#pragma once
//#include "SlateBasics.h"
//#include "PropertyEditing.h"
#include "PropertyCustomizationHelpers.h"
#include "Runtime/Slate/Public/Widgets/Layout/SConstraintCanvas.h" // can't forward declare a nested class (SConstraintCanvas::FSlot) that we need
#include "Dialogue/Base/DialogueAsset.h"
#include "Dialogue/DialogueEditorTypes.h"

// can't have objects of type SDialogueNodeWidget in DialogueViewportWidget, because SDialogueNodeWidget already contains objects of type DialogueViewportWidget, so only pointers are allowed
class SDialogueNodeWidget;
class UDialogueAsset;
//class SConstraintCanvas;
class FDialogueEditor;

// Level of detail for graph rendering (lower numbers are 'further away' with fewer details)
namespace EDialogueRenderingLOD
{
	enum Type
	{
		// Detail level when zoomed all the way out (all performance optimizations enabled)
		LowestDetail,

		// Detail level that text starts being disabled because it is unreadable
		LowDetail,

		// Detail level at which text starts to get hard to read but is still drawn
		MediumDetail,

		// Detail level when zoomed in at 1:1
		DefaultDetail,

		// Detail level when fully zoomed in (past 1:1)
		FullyZoomedIn,
	};
}


/**
* Interface for ZoomLevel values
* Provides mapping for a range of virtual ZoomLevel values to actual node scaling values
*/
struct FDialogueZoomLevelsContainer
{
	/**
	* @param InZoomLevel virtual zoom level value
	*
	* @return associated scaling value
	*/
	virtual float						GetZoomAmount(int32 InZoomLevel) const = 0;

	/**
	* @param InZoomAmount scaling value
	*
	* @return nearest ZoomLevel mapping for provided scale value
	*/
	virtual int32						GetNearestZoomLevel(float InZoomAmount) const = 0;

	/**
	* @param InZoomLevel virtual zoom level value
	*
	* @return associated friendly name
	*/
	virtual FText						GetZoomText(int32 InZoomLevel) const = 0;

	/**
	* @return count of supported zoom levels
	*/
	virtual int32						GetNumZoomLevels() const = 0;

	/**
	* @return the optimal(1:1) zoom level value, default zoom level for the graph
	*/
	virtual int32						GetDefaultZoomLevel() const = 0;

	/**
	* @param InZoomLevel virtual zoom level value
	*
	* @return associated LOD value
	*/
	virtual EDialogueRenderingLOD::Type	GetLOD(int32 InZoomLevel) const = 0;

	// Necessary for Mac OS X to compile 'delete <pointer_to_this_object>;'
	virtual ~FDialogueZoomLevelsContainer(void) {};
};

class SDialogueViewportWidget : public SCompoundWidget
{


	SLATE_BEGIN_ARGS(SDialogueViewportWidget)
		: _ShowGraphStateOverlay(true)
		, _IsEditable(true)
		{}
		SLATE_ARGUMENT(UDialogueAsset*, Dialogue)
		SLATE_ATTRIBUTE(bool, ShowGraphStateOverlay)
		SLATE_ATTRIBUTE(bool, IsEditable)
		/** Show overlay elements for the graph state such as the PIE and read-only borders and text */

	SLATE_END_ARGS()

public:

	struct ESelectionType
	{
		enum Type
		{
			WithShift,
			WithCtrl,
			Default
		};
	};

	void Construct(const FArguments& InArgs, TSharedPtr<FDialogueEditor> InDialogueEditor);
	TSharedPtr<FDialogueEditorState> GetEditingState() const { return EditingState; }
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void Tick(const FGeometry & AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry & MyGeometry, const FKeyEvent & InKeyEvent) override;
	virtual void OnFocusLost(const FFocusEvent & InFocusEvent) override;
	virtual FReply OnFocusReceived(const FGeometry & MyGeometry, const FFocusEvent & InFocusEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	/** Clears the viewport canvas, then respawns all nodes and focuses the one passed as parameter. */
	void SpawnNodes(int32 IdToFocus = 0);
	void SelectNodes(TArray<int32> NodesIds) const;
	void SelectNodes(int32 NodeId) const;
	void SelectUnderCursor() const;
	void DeselectNode(int32 NodeId) const;
	void DeselectAllNodes() const;
	void StartDraggingIndex(int32 draggedNodeIndex);
	void ForceRefresh() const;
	void DeleteSelected();
	void DeleteOneNode(int32);
	EActiveTimerReturnType EmptyTimer(double InCurrentTime, float InDeltaTime);
	/** @retun the zoom amount; e.g. a value of 0.25f results in quarter-sized nodes */
	float GetZoomAmount() const;
	FText GetZoomText() const;
	FSlateColor GetZoomTextColorAndOpacity() const;
	/** @return the view offset in graph space */
	FVector2D GetViewOffset() const;
	 /** Get the grid snap size */
	static float GetSnapGridSize()	{		return 16.f;	}
	/** @return node text size based on current zoom level*/
	FSlateFontInfo GetNodeFont();
	/** Sets node font size*/
	void UpdateFontInfo();
	/** @return text wrap length for nodes based on zoom level*/
	float GetNodeTextWrapLength();
	/** @return node min size depending on zoom level. Default is (144, 60).*/
	FVector2D GetNodeMinSize();
	void ForceSlateToStayAwake();
	TOptional<FSlateRenderTransform> GetIconScale();
	FMargin GetLeftCornerPadding();
	/** @return zoom level; e.g. values between 0 and 16 */
	int32 GetZoomLevel() const;
	int32 FindNodeIndex(int32 NodeId) const;
	void DuplicateSelected();
	void CopySelected();
	void CutSelected();
	void PasteNodes();

	ESelectionType::Type CurrentSelection = ESelectionType::Type::Default;
	mutable TArray<int32> SelectedNodes;
	mutable bool flagForRefresh;
	/** Pointer back to owning Dialogue editor instance (the keeper of state) */
	TWeakPtr<FDialogueEditor> DialogueEditorPtr;
	int32 draggedNodeIndex = -1;
	FVector2D draggingOffset;
	FVector2D panningOffset = FVector2D(0, 0);
	FVector2D CachedLocalSize = FVector2D(0, 0);
	TArray<FVector2D> CachedNodeSizes;
	TArray<TSharedPtr<SDialogueNodeWidget>> NodeWidgets;
	TArray<SConstraintCanvas::FSlot*> NodeFSlots;
	TArray<bool> isNodeVisible;
	int32 breakingLinksFromId;
	int32 EditTextNode;
	bool isLinkingAndCapturing;
	bool bBreakingLinksMode;
	bool isSelectingMultiple;
	bool isPanning;

	bool shouldTickAllWidgets = false;

	TMap<int32, int32> NodeIdsIndexes;

private:

	// Paint the background as lines
	void PaintBackgroundAsLines(const FSlateBrush* BackgroundImage, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32& DrawLayerId) const;
	/** Given a coordinate in graph space (e.g. a node's position), return the same coordinate in widget space while taking zoom and panning into account */
	FVector2D GraphCoordToPanelCoord(const FVector2D& GraphSpaceCoordinate) const;
	void OnAddNodeClicked();
	void OnIsPlayerCommited(ECheckBoxState NewState);
	void UpdateAutoPanningDirection(FVector2D& currentMouseCoords, FSlateRect& PanelScreenSpaceRect);
	void OnNodeTextCommited(const FText &InText, ETextCommit::Type);
	FMargin GetPos(int32 index);
	FText GetNodeText() const;
	ECheckBoxState GetIsPlayer() const;
	FVector2D ComputeSplineTangent(const FVector2D& Start, const FVector2D& End) const;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	// Change zoom level by the specified zoom level delta, about the specified origin.
	void ChangeZoomLevel(int32 ZoomLevelDelta, const FVector2D& WidgetSpaceZoomOrigin, bool bOverrideZoomLimiting);
	// Should be called whenever the zoom level has changed
	void PostChangedZoom();
	void UpdateZoomAffectedValues();
	EVisibility IsSimulating() const;

	/** How zoomed in/out we are. e.g. 0.25f results in quarter-sized nodes. */
	int32 ZoomLevel;
	/** Previous Zoom Level */
	int32 PreviousZoomLevel;

	// The interface for mapping ZoomLevel values to actual node scaling values
	TUniquePtr<FDialogueZoomLevelsContainer> ZoomLevels;
	TSharedPtr<FDialogueEditorState> EditingState;

	/** Current LOD level for nodes/pins */
	EDialogueRenderingLOD::Type CurrentLOD;

	bool bFirstNodeRequiresRepositionning;
	/** Whether to draw decorations for graph state (PIE / ReadOnly etc.) */
	TAttribute<bool> ShowGraphStateOverlay;
	/** Is the graph editable (can nodes be moved, etc...)? */
	TAttribute<bool> IsEditable;
	mutable TArray<int32> m_OriginallySelectedNodes;
	mutable FVector2D MarqueePointOfOrigin;
	mutable FVector2D MarqueeSize;
	mutable FVector2D MarqueeEndPoint;
	/**	Whether the software cursor should be drawn in the viewport */
	bool bShowSoftwareCursor : 1;
	FVector2D SoftwareCursorPosition;
	/** Direction for panning when we're dragging a node or a link, and the cursor is near the screen border*/
	FVector2D AutoPanningDirection = FVector2D(0, 0);
	FVector2D clickUpCoords;
	FVector2D clickDownCoords;
	FVector2D dragMouseCoords;
	/** Panning offset during marquee selection*/
	FVector2D selectionPanningOffset;
	/** Background sprite position*/
	float bgLeft = -128;
	/** Background sprite position*/
	float bgTop = -128;
	UDialogueAsset* Dialogue;
	TSharedPtr<SConstraintCanvas> CanvasPanel;
	TSharedPtr<SDialogueNodeWidget> tempWidgetReference;

	int fontSize = 9;
	FSlateFontInfo NodeFont = FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), fontSize);

	float nodeTextWrapLength = 250;

	FVector2D nodeMinSize = FVector2D(144, 60);

	/** Curve that handles fading the 'Zoom +X' text */
	FCurveSequence ZoomLevelFade;

	FIntPoint CoordsForPasting = FIntPoint::ZeroValue;
};
