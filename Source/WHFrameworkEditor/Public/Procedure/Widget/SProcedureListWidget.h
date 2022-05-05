// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ClassViewerModule.h"
#include "WHFrameworkEditorStyle.h"
#include "Procedure/ProcedureEditorTypes.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Widget/SEditorSlateWidgetBase.h"

class UProcedureBlueprintFactory;
DECLARE_DELEGATE_OneParam(FOnSelectProcedureListItemsDelegate, TArray<TSharedPtr<FProcedureListItem>>)

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SProcedureListWidget : public SEditorSlateWidgetBase
{
public:
	SLATE_BEGIN_ARGS(SProcedureListWidget) {}

		SLATE_ARGUMENT(class AProcedureModule*, ProcedureModule)

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
	TSubclassOf<UProcedureBase> SelectedProcedureClass;

	TArray<TSharedPtr<FProcedureListItem>> ProcedureListItems;

	TArray<TSharedPtr<FProcedureListItem>> SelectedProcedureListItems;

	FOnSelectProcedureListItemsDelegate OnSelectProcedureListItemsDelegate;

	bool bMultiMode = true;

	bool bEditMode = false;

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	AProcedureModule* ProcedureModule;

	UProcedureBase* CopiedProcedure;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<SScrollBar> ScrollBar;

	TSharedPtr<SComboButton> ClassPickButton;

	FClassViewerInitializationOptions ClassViewerOptions;

	TSharedPtr<FProcedureClassFilter> ProcedureClassFilter;

	TSharedPtr<STreeView<TSharedPtr<FProcedureListItem>>> TreeView;

	//////////////////////////////////////////////////////////////////////////
	/// Procedure
private:
	UProcedureBase* GenerateProcedure(TSubclassOf<UProcedureBase> InProcedureClass);

	UProcedureBase* DuplicateProcedure(UProcedureBase* InProcedure);

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
	int32 GetTotalProcedureNum() const;

	int32 GetSelectedProcedureNum() const;

	//////////////////////////////////////////////////////////////////////////
	/// Tree View
private:
	void UpdateTreeView(bool bRegenerate = false);

	void UpdateSelection();

	TSharedRef<ITableRow> GenerateTreeRow(TSharedPtr<FProcedureListItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable);

	void TreeItemScrolledIntoView(TSharedPtr<FProcedureListItem> TreeItem, const TSharedPtr<ITableRow>& Widget);

	void TreeSelectionChanged(TSharedPtr<FProcedureListItem> TreeItem, ESelectInfo::Type SelectInfo);

	void GetChildrenForTree(TSharedPtr<FProcedureListItem> TreeItem, TArray<TSharedPtr<FProcedureListItem>>& OutChildren);

	void OnTreeItemExpansionChanged(TSharedPtr<FProcedureListItem> TreeItem, bool bInExpansionState);

	void SetTreeItemExpansionRecursive(TSharedPtr<FProcedureListItem> TreeItem);

	void SetTreeItemExpansionRecursive(TSharedPtr<FProcedureListItem> TreeItem, bool bInExpansionState);

	//////////////////////////////////////////////////////////////////////////
	/// Buttons
private:
	FReply OnEditProcedureItemButtonClicked();

	FReply OnNewProcedureClassButtonClicked();

	FReply OnNewProcedureItemButtonClicked();

	FReply OnInsertProcedureItemButtonClicked();

	FReply OnAppendProcedureItemButtonClicked();

	FReply OnCopyProcedureItemButtonClicked();

	FReply OnPasteProcedureItemButtonClicked();

	FReply OnDuplicateProcedureItemButtonClicked();

	FReply OnExpandAllProcedureItemButtonClicked();

	FReply OnCollapseAllProcedureItemButtonClicked();

	FReply OnRemoveProcedureItemButtonClicked();

	FReply OnClearAllProcedureItemButtonClicked();

	FReply OnMoveUpProcedureItemButtonClicked();

	FReply OnMoveDownProcedureItemButtonClicked();
};