// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ClassViewerModule.h"
#include "Main/MainEditorTypes.h"
#include "Main/Base/ModuleBase.h"
#include "Widget/Base/SEditorWidgetBase.h"

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

		SLATE_ARGUMENT(class AMainModule*, MainModule)

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
	TSubclassOf<UModuleBase> SelectedModuleClass;

	TArray<TSharedPtr<FModuleListItem>> ModuleListItems;

	TArray<TSharedPtr<FModuleListItem>> SelectedModuleListItems;

	FOnSelectModuleListItemsDelegate OnSelectModuleListItemsDelegate;

	bool bMultiMode;

	bool bEditMode;

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	AMainModule* MainModule;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<SScrollBar> ScrollBar;

	TSharedPtr<SComboButton> ClassPickButton;

	FClassViewerInitializationOptions ClassViewerOptions;

	TSharedPtr<FModuleClassFilter> ModuleClassFilter;

	TSharedPtr<SListView<TSharedPtr<FModuleListItem>>> ListView;

	//////////////////////////////////////////////////////////////////////////
	/// Module
private:
	UModuleBase* GenerateModule(TSubclassOf<UModuleBase> InClass);

	UModuleBase* DuplicateModule(UModuleBase* InModule);

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
	int32 GetTotalModuleNum() const;

	int32 GetSelectedModuleNum() const;

	//////////////////////////////////////////////////////////////////////////
	/// List View
private:
	void UpdateListView(bool bRegenerate = false);

	void UpdateSelection();

	TSharedRef<ITableRow> GenerateListRow(TSharedPtr<FModuleListItem> ListItem, const TSharedRef<STableViewBase>& OwnerTable);

	void ListItemScrolledIntoView(TSharedPtr<FModuleListItem> ListItem, const TSharedPtr<ITableRow>& Widget);

	void ListSelectionChanged(TSharedPtr<FModuleListItem> ListItem, ESelectInfo::Type SelectInfo);

	TArray<UClass*> GetUnAddedModuleClasses() const;

	//////////////////////////////////////////////////////////////////////////
	/// Buttons
private:
	FReply OnEditModuleItemButtonClicked();

	FReply OnNewModuleClassButtonClicked();

	FReply OnAddModuleItemButtonClicked();

	FReply OnAddAllModuleItemButtonClicked();

	FReply OnInsertModuleItemButtonClicked();

	FReply OnAppendModuleItemButtonClicked();

	FReply OnRemoveModuleItemButtonClicked();

	FReply OnClearAllModuleItemButtonClicked();

	FReply OnMoveUpModuleItemButtonClicked();

	FReply OnMoveDownModuleItemButtonClicked();
};
