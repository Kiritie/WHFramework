// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Task/TaskEditor.h"
#include "Slate/Editor/Base/SEditorWidgetBase.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API STaskStatusWidget : public SEditorWidgetBase
{
public:
	STaskStatusWidget();

	SLATE_BEGIN_ARGS(STaskStatusWidget) {}

		SLATE_ARGUMENT(TWeakPtr<FTaskEditor>, TaskEditor)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

public:
	static FName WidgetName;

public:
	virtual void OnCreate() override;

	virtual void OnReset() override;

	virtual void OnRefresh() override;

	virtual void OnDestroy() override;

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	TWeakPtr<FTaskEditor> TaskEditor;
};
