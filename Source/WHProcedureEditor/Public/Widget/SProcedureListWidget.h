// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "ClassViewerModule.h"
#include "WHProcedureEditorStyle.h"
#include "Procedure/Base/ProcedureBase.h"

DECLARE_DELEGATE_OneParam(FOnSelectProcedureListItemDelegate, TArray<TSharedPtr<FProcedureListItem>>)

/**
 * 
 */
class WHPROCEDUREEDITOR_API SProcedureListWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProcedureListWidget)
	{}
	
	SLATE_ARGUMENT(class AMainModule*, MainModule)
	SLATE_ARGUMENT(class AProcedureModule*, ProcedureModule)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	//////////////////////////////////////////////////////////////////////////
	/// Stats
public:
	TSubclassOf<AProcedureBase> CurrentProcedureClass;

	TArray<TSharedPtr<FProcedureListItem>> ProcedureListItems;

	TArray<TSharedPtr<FProcedureListItem>> SelectedProcedureListItems;

	FOnSelectProcedureListItemDelegate OnSelectProcedureListItemsDelegate;

	bool bCurrentIsMultiMode = true;

	bool bCurrentIsEditMode = false;

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	class AMainModule* MainModule;
	
	class AProcedureModule* ProcedureModule;
		
	AProcedureBase* CopiedProcedure;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<SScrollBar> ScrollBar;

	TSharedPtr<SComboButton> ClassPickButton;

	FClassViewerInitializationOptions ClassViewerOptions;

	TSharedRef<FProcedureClassFilter> ProcedureClassFilter;

	TSharedPtr<STreeView<TSharedPtr<FProcedureListItem>>> TreeView;

	//////////////////////////////////////////////////////////////////////////
	/// Procedure
private:
	AProcedureBase* GenerateProcedure(TSubclassOf<AProcedureBase> InProcedureClass);

	AProcedureBase* DuplicateProcedure(AProcedureBase* InProcedure);

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

	void ToggleMultiMode();

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
