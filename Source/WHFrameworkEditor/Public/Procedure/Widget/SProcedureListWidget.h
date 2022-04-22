// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "ClassViewerModule.h"
#include "WHFrameworkEditorStyle.h"
#include "Procedure/ProcedureEditorTypes.h"
#include "Procedure/Base/ProcedureBase.h"

DECLARE_DELEGATE_OneParam(FOnSelectProcedureListItemsDelegate, TArray<TSharedPtr<FProcedureListItem>>)

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SProcedureListWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProcedureListWidget)
	{}
	
	SLATE_ARGUMENT(class AMainModule*, MainModule)
	SLATE_ARGUMENT(class AProcedureModule*, ProcedureModule)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	void Refresh();

	//////////////////////////////////////////////////////////////////////////
	/// Stats
public:
	TSubclassOf<UProcedureBase> CurrentProcedureClass;

	TArray<TSharedPtr<FProcedureListItem>> ProcedureListItems;

	TArray<TSharedPtr<FProcedureListItem>> SelectedProcedureListItems;

	FOnSelectProcedureListItemsDelegate OnSelectProcedureListItemsDelegate;

	bool bCurrentIsMultiMode = true;

	bool bCurrentIsEditMode = false;

	static FDelegateHandle RefreshDelegateHandle;

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	class AMainModule* MainModule;
	
	class AProcedureModule* ProcedureModule;
		
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
