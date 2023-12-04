// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widget/Base/SEditorWidgetBase.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SModuleStatusWidget : public SEditorWidgetBase
{
public:
	SModuleStatusWidget();
	
	SLATE_BEGIN_ARGS(SModuleStatusWidget) {}
	
		SLATE_ARGUMENT(TSharedPtr<class SModuleListWidget>, ListWidget)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

public:
	virtual void OnCreate() override;

	virtual void OnReset() override;

	virtual void OnRefresh() override;

	virtual void OnDestroy() override;

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	TSharedPtr<class SModuleListWidget> ListWidget;
};
