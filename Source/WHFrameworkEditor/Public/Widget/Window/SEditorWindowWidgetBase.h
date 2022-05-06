// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class SEditorWindowWidgetBase : public SWindow
{
public:
	SEditorWindowWidgetBase();

	SLATE_BEGIN_ARGS(SEditorWindowWidgetBase)
			: _WindowArgs() { }

		SLATE_ARGUMENT(SWindow::FArguments, WindowArgs)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

protected:
	FText WindowTitle;

	FVector2D WindowSize;
};
