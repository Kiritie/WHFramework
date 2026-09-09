#pragma once
//#include "SlateBasics.h"
//#include "PropertyEditing.h"
#include "Widgets/Text/STextBlock.h"
#include "PropertyCustomizationHelpers.h"

class SDialogueViewportWidget;
class UDialogueAsset;

class SDialogueNodeWidget : public SCompoundWidget
{
private:

	SLATE_BEGIN_ARGS(SDialogueNodeWidget){}

	SLATE_ARGUMENT(int32, Id)
	SLATE_ARGUMENT(int32, NodeIndex)
	SLATE_ARGUMENT(UDialogueAsset*, Dialogue)
	SLATE_ARGUMENT(SDialogueViewportWidget*, Owner)

	SLATE_END_ARGS()

public:

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry & AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseEnter(const FGeometry & MyGeometry, const FPointerEvent & MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry & InMyGeometry, const FPointerEvent & InMouseEvent) override;

	void BreakLinksMode();
	void BreakLinksWithNode();
	void SortParentsLinks();
	FMargin GetTextMargin() const;

	FVector2D NodeSize = FVector2D(0, 0);
	bool isSelected;
	TSharedPtr<class SMultiLineEditableText> NodeField;
	bool isVisible = true;

private:

	EVisibility GetTextFieldVisibility() const;
	EVisibility GetTextBlockVisibility() const;
	EVisibility GetEventIconVisibility() const;
	EVisibility GetConditionIconVisibility() const;
	EVisibility GetSoundIconVisibility() const;
	EVisibility GetNodeVisibility() const;
	void TextCommited(const FText& NewText, ETextCommit::Type CommitInfo, int32 id);
	void OnChangePcNpc();
	void OnBreakOutLinks();
	void OnAddPcAnswer();
	void OnAddNpcAnswer();
	void OnBreakInLinks();
	void BreakInLinks();
	void OnAddLink();
	FText GetNodeText() const;
	FText GetFieldText() const;
	const FSlateBrush* GetNodeStyle() const;

	bool bJustDoubleClicked = false;
	TSharedPtr<class STextBlock> NodeTextBlock;
	int32 Id;
	int32 NodeIndex;
	UDialogueAsset* Dialogue;
	SDialogueViewportWidget* Owner;
};
