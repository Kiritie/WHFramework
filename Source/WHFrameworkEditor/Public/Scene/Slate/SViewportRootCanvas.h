// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/Layout/SConstraintCanvas.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SViewportRootCanvas : public SConstraintCanvas
{
public:
	SViewportRootCanvas();
	
	SLATE_BEGIN_ARGS(SViewportRootCanvas) {}

		SLATE_ARGUMENT(TSharedPtr<IAssetViewport>, Viewport)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

protected:
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

protected:
	TSharedPtr<IAssetViewport> Viewport;
};
