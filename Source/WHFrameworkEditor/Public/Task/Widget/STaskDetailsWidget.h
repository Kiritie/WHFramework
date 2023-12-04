// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Task/TaskEditor.h"

#include "Task/Base/TaskBase.h"
#include "Widget/Base/SEditorWidgetBase.h"

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

public:
	virtual void OnCreate() override;

	virtual void OnReset() override;

	virtual void OnRefresh() override;

	virtual void OnDestroy() override;

	//////////////////////////////////////////////////////////////////////////
	/// Stats
public:
	TArray<TSharedPtr<FTaskListItem>> SelectedTaskListItems;

	TWeakPtr<FTaskEditor> TaskEditor;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<IDetailsView> DetailsView;

private:
	void UpdateDetailsView();

	void OnSelectTaskListItem(TArray<TSharedPtr<FTaskListItem>> TaskListItem);
};
