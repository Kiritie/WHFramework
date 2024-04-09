// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

class SEditorWidgetBase;

/**
 * Enumerates widget orientations.
 */
UENUM(BlueprintType)
enum EEditorDrawerOrientation : int
{
	DrawerOrient_LeftToRight UMETA(DisplayName="LeftToRight"),
	DrawerOrient_RightToLeft UMETA(DisplayName="RightToLeft"),
	DrawerOrient_BottomToTop UMETA(DisplayName="BottomToTop"),
	DrawerOrient_TopToBottom UMETA(DisplayName="TopToBottom"),
};

class WHFRAMEWORKSLATE_API SEditorDrawerPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEditorDrawerPanel)
		:_AutoOpen(false)
	{}

		SLATE_ARGUMENT(EEditorDrawerOrientation, Orientation)
		SLATE_ARGUMENT(float, Duration)
		SLATE_ARGUMENT(ECurveEaseFunction, Ease)
		SLATE_ARGUMENT(bool, AutoOpen)
		SLATE_ATTRIBUTE(FOptionalSize, ContentWidth)
		SLATE_ATTRIBUTE(FOptionalSize, ContentHeight)
		SLATE_ATTRIBUTE(FMargin, ContentPadding)
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_NAMED_SLOT(FArguments, EdgeContent)
		SLATE_ATTRIBUTE(FMargin, EdgePadding)

	SLATE_END_ARGS()

	SEditorDrawerPanel();

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

public:
	void Open();

	void Close();

	void Toggle();

public:
	bool IsOpened() const;
	
	bool IsClosed() const;

protected:
	FReply OnOpenButtonClicked();

protected:
	TOptional<FSlateRenderTransform> GetDrawerRenderTransform() const;

private:
	EEditorDrawerOrientation Orientation;
	TAttribute<FOptionalSize> ContentWidth;
	TAttribute<FOptionalSize> ContentHeight;
	TAttribute<FMargin> ContentPadding;

	FCurveSequence OpenSequence;
};
