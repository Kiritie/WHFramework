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
	SProcedureListWidget();
	
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

	bool bMultiMode;

	bool bEditMode;

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	AProcedureModule* ProcedureModule;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<SScrollBar> ScrollBar;

	TSharedPtr<SComboButton> ClassPickButton;

	FClassViewerInitializationOptions ClassViewerOptions;

	TSharedPtr<FProcedureClassFilter> ProcedureClassFilter;

	TSharedPtr<SListView<TSharedPtr<FProcedureListItem>>> ListView;

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
	/// List View
private:
	void UpdateListView(bool bRegenerate = false);

	void UpdateSelection();

	TSharedRef<ITableRow> GenerateListRow(TSharedPtr<FProcedureListItem> ListItem, const TSharedRef<STableViewBase>& OwnerTable);

	void ListItemScrolledIntoView(TSharedPtr<FProcedureListItem> ListItem, const TSharedPtr<ITableRow>& Widget);

	void ListSelectionChanged(TSharedPtr<FProcedureListItem> ListItem, ESelectInfo::Type SelectInfo);

	//////////////////////////////////////////////////////////////////////////
	/// Buttons
private:
	FReply OnEditProcedureItemButtonClicked();

	FReply OnNewProcedureClassButtonClicked();

	FReply OnNewProcedureItemButtonClicked();

	FReply OnInsertProcedureItemButtonClicked();

	FReply OnAppendProcedureItemButtonClicked();

	FReply OnRemoveProcedureItemButtonClicked();

	FReply OnClearAllProcedureItemButtonClicked();

	FReply OnMoveUpProcedureItemButtonClicked();

	FReply OnMoveDownProcedureItemButtonClicked();
};
