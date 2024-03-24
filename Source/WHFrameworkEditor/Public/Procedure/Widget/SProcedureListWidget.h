// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ClassViewerModule.h"
#include "Procedure/ProcedureEditorTypes.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Base/SEditorWidgetBase.h"

class UProcedureBlueprintFactory;
DECLARE_DELEGATE_OneParam(FOnSelectProcedureListItemsDelegate, TArray<TSharedPtr<FProcedureListItem>>)

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SProcedureListWidget : public SEditorWidgetBase
{
public:
	SProcedureListWidget();
	
	SLATE_BEGIN_ARGS(SProcedureListWidget) {}

		SLATE_ARGUMENT(TWeakPtr<FProcedureEditor>, ProcedureEditor)

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
	/// Procedure
private:
	UProcedureBase* GenerateProcedure(TSubclassOf<UProcedureBase> InClass);

	UProcedureBase* DuplicateProcedure(UProcedureBase* InProcedure);

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
	int32 GetTotalProcedureNum() const;

	int32 GetSelectedProcedureNum() const;

	//////////////////////////////////////////////////////////////////////////
	/// List View
private:
	void UpdateListView(bool bRegenerate = false);

	void UpdateSelection();

	void ClearSelection();

	TSharedRef<ITableRow> GenerateListRow(TSharedPtr<FProcedureListItem> ListItem, const TSharedRef<STableViewBase>& OwnerTable);

	void ListItemScrolledIntoView(TSharedPtr<FProcedureListItem> ListItem, const TSharedPtr<ITableRow>& Widget);

	void ListSelectionChanged(TSharedPtr<FProcedureListItem> ListItem, ESelectInfo::Type SelectInfo);

	void SetListItemSelectionRecursive(TSharedPtr<FProcedureListItem> ListItem);

	//////////////////////////////////////////////////////////////////////////
	/// Buttons
private:
	FReply OnEditProcedureItemButtonClicked();

	FReply OnNewProcedureClassButtonClicked();

	FReply OnAddProcedureItemButtonClicked();

	FReply OnInsertProcedureItemButtonClicked();

	FReply OnAppendProcedureItemButtonClicked();

	FReply OnRemoveProcedureItemButtonClicked();

	FReply OnClearAllProcedureItemButtonClicked();

	FReply OnMoveUpProcedureItemButtonClicked();

	FReply OnMoveDownProcedureItemButtonClicked();

	//////////////////////////////////////////////////////////////////////////
	/// Stats
public:
	bool bDefaults;

	bool bEditing;
	
	FText ActiveFilterText;

	TWeakPtr<FProcedureEditor> ProcedureEditor;

	TSubclassOf<UProcedureBase> SelectedProcedureClass;

	TArray<TSharedPtr<FProcedureListItem>> ProcedureListItems;

	TArray<TSharedPtr<FProcedureListItem>> VisibleProcedureListItems;

	TArray<TSharedPtr<FProcedureListItem>> SelectedProcedureListItems;

	FOnSelectProcedureListItemsDelegate OnSelectProcedureListItemsDelegate;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<SScrollBar> ScrollBar;

	TSharedPtr<SComboButton> ClassPickButton;

	FClassViewerInitializationOptions ClassViewerOptions;

	TSharedPtr<FProcedureClassFilter> ProcedureClassFilter;

	TSharedPtr<SListView<TSharedPtr<FProcedureListItem>>> ListView;

	TSharedPtr<SSearchBox> SearchBox;
};
