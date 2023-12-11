// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ClassViewerModule.h"
#include "Task/TaskEditorTypes.h"
#include "Task/Base/TaskBase.h"
#include "Widget/Base/SEditorWidgetBase.h"

class UTaskBlueprintFactory;
DECLARE_DELEGATE_OneParam(FOnSelectTaskListItemsDelegate, TArray<TSharedPtr<FTaskListItem>>)

/**
 * 
 */
class WHFRAMEWORKEDITOR_API STaskListWidget : public SEditorWidgetBase
{
public:
	STaskListWidget();
	
	SLATE_BEGIN_ARGS(STaskListWidget) {}

		SLATE_ARGUMENT(TWeakPtr<FTaskEditor>, TaskEditor)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

public:
	virtual void OnCreate() override;

	virtual void OnInitialize() override;

	virtual void OnReset() override;

	virtual void OnRefresh() override;

	virtual void OnDestroy() override;

protected:
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	//////////////////////////////////////////////////////////////////////////
	/// Task
private:
	UTaskBase* GenerateTask(TSubclassOf<UTaskBase> InClass);

	UTaskBase* DuplicateTask(UTaskBase* InTask);

	//////////////////////////////////////////////////////////////////////////
	/// Class Picker
private:
	TSharedRef<SWidget> GenerateClassPicker();

	void OnClassPicked(UClass* InClass);

	FText GetPickedClassName() const;

	//////////////////////////////////////////////////////////////////////////
	/// Search Box
private:
	FText GetFilterText() const;

	void OnFilterTextChanged(const FText& InFilterText);

	void OnFilterTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

	//////////////////////////////////////////////////////////////////////////
	/// Mode
public:
	void ToggleEditing();

	void SetIsEditing(bool bIsEditing);

	void ToggleDefaults();

	void SetIsDefaults(bool bIsDefaults);

	//////////////////////////////////////////////////////////////////////////
	/// Num
public:
	int32 GetTotalTaskNum() const;

	int32 GetSelectedTaskNum() const;

	//////////////////////////////////////////////////////////////////////////
	/// Tree View
private:
	void UpdateTreeView(bool bRegenerate = false);

	void UpdateSelection();

	TSharedRef<ITableRow> GenerateTreeRow(TSharedPtr<FTaskListItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable);

	void TreeItemScrolledIntoView(TSharedPtr<FTaskListItem> TreeItem, const TSharedPtr<ITableRow>& Widget);

	void TreeSelectionChanged(TSharedPtr<FTaskListItem> TreeItem, ESelectInfo::Type SelectInfo);

	void GetChildrenForTree(TSharedPtr<FTaskListItem> TreeItem, TArray<TSharedPtr<FTaskListItem>>& OutChildren);

	void SetTreeItemSelectionRecursive(TSharedPtr<FTaskListItem> TreeItem);

	void TreeItemExpansionChanged(TSharedPtr<FTaskListItem> TreeItem, bool bInExpansionState);

	void SetTreeItemExpansionRecursive(TSharedPtr<FTaskListItem> TreeItem);

	void SetTreeItemExpansionRecursive(TSharedPtr<FTaskListItem> TreeItem, bool bInExpansionState);

	//////////////////////////////////////////////////////////////////////////
	/// Buttons
private:
	FReply OnEditTaskItemButtonClicked();

	FReply OnNewTaskClassButtonClicked();

	FReply OnAddTaskItemButtonClicked();

	FReply OnInsertTaskItemButtonClicked();

	FReply OnAppendTaskItemButtonClicked();

	FReply OnCopyTaskItemButtonClicked();

	FReply OnPasteTaskItemButtonClicked();

	FReply OnDuplicateTaskItemButtonClicked();

	FReply OnExpandAllTaskItemButtonClicked();

	FReply OnCollapseAllTaskItemButtonClicked();

	FReply OnRemoveTaskItemButtonClicked();

	FReply OnClearAllTaskItemButtonClicked();

	FReply OnMoveUpTaskItemButtonClicked();

	FReply OnMoveDownTaskItemButtonClicked();

	//////////////////////////////////////////////////////////////////////////
	/// Stats
public:
	bool bDefaults;

	bool bEditing;
		
	FText ActiveFilterText;

	TWeakPtr<FTaskEditor> TaskEditor;

	TSubclassOf<UTaskBase> SelectedTaskClass;

	TArray<TSharedPtr<FTaskListItem>> TaskListItems;

	TArray<TSharedPtr<FTaskListItem>> VisibleTaskListItems;

	TArray<TSharedPtr<FTaskListItem>> SelectedTaskListItems;

	FOnSelectTaskListItemsDelegate OnSelectTaskListItemsDelegate;

	UTaskBase* CopiedTask;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<SScrollBar> ScrollBar;

	TSharedPtr<SComboButton> ClassPickButton;

	FClassViewerInitializationOptions ClassViewerOptions;

	TSharedPtr<FTaskClassFilter> TaskClassFilter;

	TSharedPtr<STreeView<TSharedPtr<FTaskListItem>>> TreeView;

	TSharedPtr<SSearchBox> SearchBox;
};
