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
	virtual void OnWindowActivated();
	
	virtual void OnWindowDeactivated();
	
	virtual void OnWindowClosed(const TSharedRef<SWindow>& InWindow);

private:
	FDelegateHandle OnWindowActivatedHandle;
	FDelegateHandle OnWindowDeactivatedHandle;
	FDelegateHandle OnWindowClosedHandle;

public:
	virtual void OnCreate();

	virtual void OnReset();

	virtual void OnRefresh();

	virtual void OnDestroy();

public:
	virtual void Reset();

	virtual void Refresh();

	virtual void Destroy();

protected:
	FText WindowTitle;

	FVector2D WindowSize;
};
