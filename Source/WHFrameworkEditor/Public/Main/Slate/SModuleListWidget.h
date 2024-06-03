// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ClassViewerModule.h"
#include "Main/MainEditorTypes.h"
#include "Main/Base/ModuleBase.h"
#include "Slate/Editor/Base/SEditorWidgetBase.h"

class UModuleBlueprintFactory;
DECLARE_DELEGATE_OneParam(FOnSelectModuleListItemsDelegate, TArray<TSharedPtr<FModuleListItem>>)

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SModuleListWidget : public SEditorWidgetBase
{
public:
	SModuleListWidget();
	
	SLATE_BEGIN_ARGS(SModuleListWidget) {}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	static const FName WidgetName;

public:
	virtual void OnCreate() override;

	virtual void OnInitialize() override;

	virtual void OnReset() override;

	virtual void OnRefresh() override;

	virtual void OnDestroy() override;

protected:
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	//////////////////////////////////////////////////////////////////////////
	/// ModuleBase
private:
	UModuleBase* GenerateModule(TSubclassOf<UModuleBase> InClass);

	UModuleBase* DuplicateModule(UModuleBase* InModule);

	void GenerateRequireModules();

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
	int32 GetTotalModuleNum() const;

	int32 GetSelectedModuleNum() const;

	TArray<UClass*> GetUnAddedModuleClasses() const;

	//////////////////////////////////////////////////////////////////////////
	/// List View
private:
	void UpdateListView(bool bRegenerate = false);

	void UpdateSelection();

	void ClearSelection();

	TSharedRef<ITableRow> GenerateListRow(TSharedPtr<FModuleListItem> ListItem, const TSharedRef<STableViewBase>& OwnerTable);

	void ListItemScrolledIntoView(TSharedPtr<FModuleListItem> ListItem, const TSharedPtr<ITableRow>& Widget);

	void ListSelectionChanged(TSharedPtr<FModuleListItem> ListItem, ESelectInfo::Type SelectInfo);

	void SetListItemSelectionRecursive(TSharedPtr<FModuleListItem> ListItem);

	//////////////////////////////////////////////////////////////////////////
	/// Buttons
private:
	FReply OnEditModuleItemButtonClicked();

	FReply OnNewModuleClassButtonClicked();

	FReply OnAddModuleItemButtonClicked();

	FReply OnAddAllModuleItemButtonClicked();

	FReply OnInsertModuleItemButtonClicked();

	FReply OnAppendModuleItemButtonClicked();

	FReply OnRefreshModuleItemButtonClicked();

	FReply OnRemoveModuleItemButtonClicked();

	FReply OnClearAllModuleItemButtonClicked();

	FReply OnMoveUpModuleItemButtonClicked();

	FReply OnMoveDownModuleItemButtonClicked();

	//////////////////////////////////////////////////////////////////////////
	/// Stats
public:
	bool bDefaults;

	bool bEditing;
	
	FText ActiveFilterText;

	TSubclassOf<UModuleBase> SelectedModuleClass;

	TArray<TSharedPtr<FModuleListItem>> ModuleListItems;

	TArray<TSharedPtr<FModuleListItem>> VisibleModuleListItems;

	TArray<TSharedPtr<FModuleListItem>> SelectedModuleListItems;

	FOnSelectModuleListItemsDelegate OnSelectModuleListItemsDelegate;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<SScrollBar> ScrollBar;

	TSharedPtr<SComboButton> ClassPickButton;

	FClassViewerInitializationOptions ClassViewerOptions;

	TSharedPtr<FModuleClassFilter> ModuleClassFilter;

	TSharedPtr<SListView<TSharedPtr<FModuleListItem>>> ListView;

	TSharedPtr<SSearchBox> SearchBox;
};
