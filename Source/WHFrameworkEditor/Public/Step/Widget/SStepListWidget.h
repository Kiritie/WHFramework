// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ClassViewerModule.h"
#include "Step/StepEditorTypes.h"
#include "Step/Base/StepBase.h"
#include "Widget/Base/SEditorWidgetBase.h"

class UStepBlueprintFactory;
DECLARE_DELEGATE_OneParam(FOnSelectStepListItemsDelegate, TArray<TSharedPtr<FStepListItem>>)

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SStepListWidget : public SEditorWidgetBase
{
public:
	SStepListWidget();
	
	SLATE_BEGIN_ARGS(SStepListWidget) {}

		SLATE_ARGUMENT(TWeakPtr<FStepEditor>, StepEditor)

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
	/// Step
private:
	UStepBase* GenerateStep(TSubclassOf<UStepBase> InClass);

	UStepBase* DuplicateStep(UStepBase* InStep);

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
	int32 GetTotalStepNum() const;

	int32 GetSelectedStepNum() const;

	//////////////////////////////////////////////////////////////////////////
	/// Tree View
private:
	void UpdateTreeView(bool bRegenerate = false);

	void UpdateSelection();

	void ClearSelection();

	TSharedRef<ITableRow> GenerateTreeRow(TSharedPtr<FStepListItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable);

	void TreeItemScrolledIntoView(TSharedPtr<FStepListItem> TreeItem, const TSharedPtr<ITableRow>& Widget);

	void TreeSelectionChanged(TSharedPtr<FStepListItem> TreeItem, ESelectInfo::Type SelectInfo);

	void GetChildrenForTree(TSharedPtr<FStepListItem> TreeItem, TArray<TSharedPtr<FStepListItem>>& OutChildren);

	void SetTreeItemSelectionRecursive(TSharedPtr<FStepListItem> TreeItem);

	void TreeItemExpansionChanged(TSharedPtr<FStepListItem> TreeItem, bool bInExpansionState);

	void SetTreeItemExpansionRecursive(TSharedPtr<FStepListItem> TreeItem);

	void SetTreeItemExpansionRecursive(TSharedPtr<FStepListItem> TreeItem, bool bInExpansionState);

	//////////////////////////////////////////////////////////////////////////
	/// Buttons
private:
	FReply OnEditStepItemButtonClicked();

	FReply OnNewStepClassButtonClicked();

	FReply OnAddStepItemButtonClicked();

	FReply OnInsertStepItemButtonClicked();

	FReply OnAppendStepItemButtonClicked();

	FReply OnCopyStepItemButtonClicked();

	FReply OnPasteStepItemButtonClicked();

	FReply OnDuplicateStepItemButtonClicked();

	FReply OnExpandAllStepItemButtonClicked();

	FReply OnCollapseAllStepItemButtonClicked();

	FReply OnRemoveStepItemButtonClicked();

	FReply OnClearAllStepItemButtonClicked();

	FReply OnMoveUpStepItemButtonClicked();

	FReply OnMoveDownStepItemButtonClicked();

	//////////////////////////////////////////////////////////////////////////
	/// Stats
public:
	bool bDefaults;

	bool bEditing;
	
	FText ActiveFilterText;

	TSubclassOf<UStepBase> SelectedStepClass;

	TArray<TSharedPtr<FStepListItem>> StepListItems;

	TArray<TSharedPtr<FStepListItem>> VisibleStepListItems;

	TArray<TSharedPtr<FStepListItem>> SelectedStepListItems;

	FOnSelectStepListItemsDelegate OnSelectStepListItemsDelegate;

	TWeakPtr<FStepEditor> StepEditor;

	UStepBase* CopiedStep;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<SScrollBar> ScrollBar;

	TSharedPtr<SComboButton> ClassPickButton;

	FClassViewerInitializationOptions ClassViewerOptions;

	TSharedPtr<FStepClassFilter> StepClassFilter;

	TSharedPtr<STreeView<TSharedPtr<FStepListItem>>> TreeView;

	TSharedPtr<SSearchBox> SearchBox;
};
