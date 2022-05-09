// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ClassViewerModule.h"
#include "WHFrameworkEditorStyle.h"
#include "Step/StepEditorTypes.h"
#include "Step/Base/StepBase.h"
#include "Widget/SEditorSlateWidgetBase.h"

class UStepBlueprintFactory;
DECLARE_DELEGATE_OneParam(FOnSelectStepListItemsDelegate, TArray<TSharedPtr<FStepListItem>>)

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SStepListWidget : public SEditorSlateWidgetBase
{
public:
	SStepListWidget();
	
	SLATE_BEGIN_ARGS(SStepListWidget) {}

		SLATE_ARGUMENT(class AStepModule*, StepModule)

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
	TSubclassOf<UStepBase> SelectedStepClass;

	TArray<TSharedPtr<FStepListItem>> StepListItems;

	TArray<TSharedPtr<FStepListItem>> SelectedStepListItems;

	FOnSelectStepListItemsDelegate OnSelectStepListItemsDelegate;

	bool bMultiMode = true;

	bool bEditMode = false;

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	AStepModule* StepModule;

	UStepBase* CopiedStep;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<SScrollBar> ScrollBar;

	TSharedPtr<SComboButton> ClassPickButton;

	FClassViewerInitializationOptions ClassViewerOptions;

	TSharedPtr<FStepClassFilter> StepClassFilter;

	TSharedPtr<STreeView<TSharedPtr<FStepListItem>>> TreeView;

	//////////////////////////////////////////////////////////////////////////
	/// Step
private:
	UStepBase* GenerateStep(TSubclassOf<UStepBase> InStepClass);

	UStepBase* DuplicateStep(UStepBase* InStep);

	//////////////////////////////////////////////////////////////////////////
	/// Class Picker
private:
	TSharedRef<SWidget> GenerateClassPicker();

	void OnClassPicked(UClass* InClass);

	FText GetPickedClassName() const;

	//////////////////////////////////////////////////////////////////////////
	/// Mode
public:
	void ToggleEditMode();

	void SetIsEditMode(bool bIsEditMode);

	void ToggleMultiMode();

	void SetIsMultiMode(bool bIsMultiMode);

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

	TSharedRef<ITableRow> GenerateTreeRow(TSharedPtr<FStepListItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable);

	void TreeItemScrolledIntoView(TSharedPtr<FStepListItem> TreeItem, const TSharedPtr<ITableRow>& Widget);

	void TreeSelectionChanged(TSharedPtr<FStepListItem> TreeItem, ESelectInfo::Type SelectInfo);

	void GetChildrenForTree(TSharedPtr<FStepListItem> TreeItem, TArray<TSharedPtr<FStepListItem>>& OutChildren);

	void OnTreeItemExpansionChanged(TSharedPtr<FStepListItem> TreeItem, bool bInExpansionState);

	void SetTreeItemExpansionRecursive(TSharedPtr<FStepListItem> TreeItem);

	void SetTreeItemExpansionRecursive(TSharedPtr<FStepListItem> TreeItem, bool bInExpansionState);

	//////////////////////////////////////////////////////////////////////////
	/// Buttons
private:
	FReply OnEditStepItemButtonClicked();

	FReply OnNewStepClassButtonClicked();

	FReply OnNewStepItemButtonClicked();

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
};
