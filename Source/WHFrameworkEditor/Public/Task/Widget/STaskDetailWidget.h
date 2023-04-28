// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Task/Base/TaskBase.h"
#include "Widget/SEditorSlateWidgetBase.h"

class STaskListWidget;
class ATaskModule;
/**
 * 
 */
class WHFRAMEWORKEDITOR_API STaskDetailWidget : public SEditorSlateWidgetBase
{
public:
	STaskDetailWidget();
	
	SLATE_BEGIN_ARGS(STaskDetailWidget) {}

		SLATE_ARGUMENT(ATaskModule*, TaskModule)

		SLATE_ARGUMENT(TSharedPtr<STaskListWidget>, ListWidget)

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

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	ATaskModule* TaskModule;

	TSharedPtr<STaskListWidget> ListWidget;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<IDetailsView> DetailsView;

private:
	void UpdateDetailsView();

	void OnSelectTaskListItem(TArray<TSharedPtr<FTaskListItem>> TaskListItem);
};
