// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Task/TaskEditor.h"

#include "Slate/Editor/Base/SEditorWidgetBase.h"

class STaskListWidget;
class UTaskModule;
/**
 * 
 */
class WHFRAMEWORKEDITOR_API STaskDetailsWidget : public SEditorWidgetBase
{
public:
	STaskDetailsWidget();
	
	SLATE_BEGIN_ARGS(STaskDetailsWidget) {}

		SLATE_ARGUMENT(TWeakPtr<FTaskEditor>, TaskEditor)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	static const FName WidgetName;

public:
	virtual void OnCreate() override;

	virtual void OnReset() override;

	virtual void OnRefresh() override;

	virtual void OnDestroy() override;

	//////////////////////////////////////////////////////////////////////////
	/// Stats
public:
	TWeakPtr<FTaskEditor> TaskEditor;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<IDetailsView> DetailsView;
};
