// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

enum class EPanelState : uint8
{
	None,
	Opened,
	Closed
};

class SBasePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBasePanel) {}

	SLATE_END_ARGS()

	SBasePanel();

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

protected:
	virtual void OnOpen();

	virtual void OnClose();

	virtual void OnRefresh();

public:
	virtual void Open();

	virtual void Close();

	virtual void Toggle();

	virtual void Refresh();

protected:
	EPanelState PanelState;
};
