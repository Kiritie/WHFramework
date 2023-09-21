// Fill out your copyright notice in the Description page of Project Settings.


#include "Task/Widget/STaskListWidget.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "SlateOptMacros.h"
#include "SourceCodeNavigation.h"
#include "Common/Blueprint/Widget/SCreateBlueprintAssetDialog.h"
#include "Task/TaskBlueprintFactory.h"
#include "Task/TaskEditorTypes.h"
#include "Task/TaskModule.h"
#include "Task/Base/TaskBlueprint.h"
#include "Task/Base/TaskBase.h"
#include "Task/Widget/STaskListItemWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

STaskListWidget::STaskListWidget()
{
	WidgetName = FName("TaskListWidget");
	WidgetType = EEditorWidgetType::Child;

	bMultiMode = true;
	bEditMode = false;

	TaskModule = nullptr;
	CopiedTask = nullptr;
}

void STaskListWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());

	TaskModule = InArgs._TaskModule;

	if(!TaskModule) return;

	if(!TreeView.IsValid())
	{
		SAssignNew(TreeView, STreeView< TSharedPtr<FTaskListItem> >)
			.TreeItemsSource(&TaskListItems)
			.OnGenerateRow(this, &STaskListWidget::GenerateTreeRow)
			.OnItemScrolledIntoView(this, &STaskListWidget::TreeItemScrolledIntoView)
			.ItemHeight(18)
			.SelectionMode_Lambda([this](){ return bMultiMode ? ESelectionMode::Multi : ESelectionMode::SingleToggle; })
			.OnSelectionChanged(this, &STaskListWidget::TreeSelectionChanged)
			.OnGetChildren(this, &STaskListWidget::GetChildrenForTree)
			.OnExpansionChanged(this, &STaskListWidget::OnTreeItemExpansionChanged)
			.OnSetExpansionRecursive(this, &STaskListWidget::SetTreeItemExpansionRecursive)
			.ClearSelectionOnClick(false)
			.HighlightParentNodesForSelection(true);
	}

	if(!ScrollBar.IsValid())
	{
		SAssignNew(ScrollBar, SScrollBar)
			.Thickness(FVector2D(9.0f, 9.0f));
	}

	SelectedTaskClass = UTaskBase::StaticClass();;

	ClassViewerOptions.bShowBackgroundBorder = false;
	ClassViewerOptions.bShowUnloadedBlueprints = true;
	ClassViewerOptions.bShowNoneOption = false;

	ClassViewerOptions.bIsBlueprintBaseOnly = false;
	ClassViewerOptions.bIsPlaceableOnly = false;
	ClassViewerOptions.NameTypeToDisplay = EClassViewerNameTypeToDisplay::ClassName;
	ClassViewerOptions.DisplayMode = EClassViewerDisplayMode::ListView;
	ClassViewerOptions.bAllowViewOptions = true;

	TaskClassFilter = MakeShareable(new FTaskClassFilter);
	TaskClassFilter->IncludeParentClass = UTaskBase::StaticClass();
	#if ENGINE_MAJOR_VERSION == 4
	ClassViewerOptions.ClassFilter = TaskClassFilter;
	#else if ENGINE_MAJOR_VERSION == 5
	ClassViewerOptions.ClassFilters.Add(TaskClassFilter.ToSharedRef());
	#endif

	SAssignNew(ClassPickButton, SComboButton)
		.OnGetMenuContent(this, &STaskListWidget::GenerateClassPicker)
		.ContentPadding(FMargin(2.0f, 2.0f))
		.ToolTipText(this, &STaskListWidget::GetPickedClassName)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &STaskListWidget::GetPickedClassName)
		];

	ChildSlot
	[
		SNew(SBorder)
		.Padding(1.f)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SBox)
			.WidthOverride(420)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Fill)
					.Padding(2.f, 0, 0, 0)
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT(" Class: ")))
						.ColorAndOpacity(FSlateColor(FLinearColor::Yellow))
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					.FillWidth(1)
					[
						ClassPickButton.ToSharedRef()
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Fill)
					.AutoWidth()
					.Padding(2.f, 0, 0, 0)
					[
						SNew(SButton)
						.ContentPadding(FMargin(2.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("New")))
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &STaskListWidget::OnNewTaskClassButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Fill)
					.AutoWidth()
					.Padding(2.f, 0, 0, 0)
					[
						SNew(SButton)
						.ContentPadding(FMargin(2.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Edit")))
						.IsEnabled_Lambda([this](){ return SelectedTaskClass != nullptr; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &STaskListWidget::OnEditTaskItemButtonClicked)
					]
				]

				+ SVerticalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.AutoHeight()
				.Padding(0, 2.f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Add")))
						.IsEnabled_Lambda([this](){ return !bEditMode && SelectedTaskListItems.Num() <= 1 && SelectedTaskClass; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &STaskListWidget::OnAddTaskItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Insert")))
						.IsEnabled_Lambda([this](){ return !bEditMode && SelectedTaskListItems.Num() == 1 && SelectedTaskClass; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &STaskListWidget::OnInsertTaskItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Append")))
						.IsEnabled_Lambda([this](){ return !bEditMode && SelectedTaskListItems.Num() == 1 && SelectedTaskClass; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &STaskListWidget::OnAppendTaskItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Copy")))
						.IsEnabled_Lambda([this](){ return SelectedTaskListItems.Num() == 1; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &STaskListWidget::OnCopyTaskItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Paste")))
						.IsEnabled_Lambda([this](){ return CopiedTask != nullptr; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &STaskListWidget::OnPasteTaskItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Duplicate")))
						.IsEnabled_Lambda([this](){ return SelectedTaskListItems.Num() == 1; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &STaskListWidget::OnDuplicateTaskItemButtonClicked)
					]
				]

				+ SVerticalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.FillHeight(1)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					.FillWidth(1)
					[
						TreeView.ToSharedRef()
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						ScrollBar.ToSharedRef()
					]
				]

				+ SVerticalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.AutoHeight()
				.Padding(0, 2.f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Expand")))
						.IsEnabled_Lambda([this](){ return TaskModule->GetRootTasks().Num() > 0; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &STaskListWidget::OnExpandAllTaskItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Collapse")))
						.IsEnabled_Lambda([this](){ return TaskModule->GetRootTasks().Num() > 0; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &STaskListWidget::OnCollapseAllTaskItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Clear All")))
						.IsEnabled_Lambda([this](){ return TaskModule->GetRootTasks().Num() > 0; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &STaskListWidget::OnClearAllTaskItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Remove")))
						.IsEnabled_Lambda([this](){ return SelectedTaskListItems.Num() > 0; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &STaskListWidget::OnRemoveTaskItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Move Up")))
						.IsEnabled_Lambda([this](){ return SelectedTaskListItems.Num() == 1 && SelectedTaskListItems[0]->GetTaskIndex() > 0; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &STaskListWidget::OnMoveUpTaskItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Move Down")))
						.IsEnabled_Lambda([this](){
							return SelectedTaskListItems.Num() == 1 &&
								SelectedTaskListItems[0]->GetTaskIndex() < (SelectedTaskListItems[0]->GetParentTask() ? SelectedTaskListItems[0]->GetParentSubTasks().Num() : TaskModule->GetRootTasks().Num()) - 1;
						})
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &STaskListWidget::OnMoveDownTaskItemButtonClicked)
					]
				]
			]
		]
	];

	GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.TaskEditorSettings"), TEXT("bMultiMode"), bMultiMode, GTaskEditorIni);
	GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.TaskEditorSettings"), TEXT("bEditMode"), bEditMode, GTaskEditorIni);

	UpdateTreeView(true);

	SetIsMultiMode(bMultiMode);
	SetIsEditMode(bEditMode);
}

void STaskListWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();
}

void STaskListWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void STaskListWidget::OnRefresh()
{
	SEditorWidgetBase::OnRefresh();

	UpdateTreeView(true);
	UpdateSelection();
	SetIsEditMode(bEditMode);
}

void STaskListWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

UTaskBase* STaskListWidget::GenerateTask(TSubclassOf<UTaskBase> InClass)
{
	UTaskBase* NewTask = NewObject<UTaskBase>(TaskModule, InClass, NAME_None);

	// NewTask->TaskName = *CurrentTaskClass->GetName().Replace(TEXT("Task_"), TEXT(""));
	// NewTask->TaskDisplayName = FText::FromName(NewTask->TaskName);

	NewTask->OnGenerate();

	return NewTask;
}

UTaskBase* STaskListWidget::DuplicateTask(UTaskBase* InTask)
{
	UTaskBase* NewTask = DuplicateObject<UTaskBase>(InTask, TaskModule, NAME_None);

	NewTask->TaskGUID = FGuid::NewGuid().ToString();
	NewTask->SubTasks.Empty();

	// NewTask->TaskName = *FString::Printf(TEXT("%s_Copy"), *NewTask->TaskName.ToString());
	// NewTask->TaskDisplayName = FText::FromString(FString::Printf(TEXT("%s_Copy"), *NewTask->TaskDisplayName.ToString()));

	NewTask->OnGenerate();

	return NewTask;
}

TSharedRef<SWidget> STaskListWidget::GenerateClassPicker()
{
	FOnClassPicked OnPicked(FOnClassPicked::CreateRaw(this, &STaskListWidget::OnClassPicked));

	return SNew(SBox)
		.WidthOverride(280)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(500)
			[
				FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(ClassViewerOptions, OnPicked)
			]
		];
}

void STaskListWidget::OnClassPicked(UClass* InClass)
{
	ClassPickButton->SetIsOpen(false);

	if(!bEditMode)
	{
		SelectedTaskClass = InClass;
	}
	else if(SelectedTaskListItems.Num() > 0)
	{
		if(FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("Are you sure to change selected tasks class?"))) == EAppReturnType::Yes)
		{
			for(int32 i = 0; i < SelectedTaskListItems.Num(); i++)
			{
				UTaskBase* OldTask = SelectedTaskListItems[i]->Task;
				UTaskBase* NewTask = GenerateTask(InClass);

				if(NewTask && OldTask)
				{
					UCommonBPLibrary::ExportPropertiesToObject(OldTask, NewTask);
					if(OldTask->ParentTask)
					{
						OldTask->ParentTask->SubTasks[OldTask->TaskIndex] = NewTask;
					}
					else
					{
						TaskModule->GetRootTasks().EmplaceAt(OldTask->TaskIndex, NewTask);
					}
					TaskModule->GetTaskMap().Emplace(OldTask->TaskGUID, NewTask);
					OldTask->OnUnGenerate();
					Refresh();
				}
			}

			TaskModule->Modify();

			OnSelectTaskListItemsDelegate.Execute(SelectedTaskListItems);
			SelectedTaskClass = InClass;
		}
	}
}

FText STaskListWidget::GetPickedClassName() const
{
	return FText::FromString(SelectedTaskClass ? SelectedTaskClass->GetName() : TEXT("None"));
}

void STaskListWidget::ToggleEditMode()
{
	SetIsEditMode(!bEditMode);
}

void STaskListWidget::SetIsEditMode(bool bIsEditMode)
{
	bEditMode = bIsEditMode;
	if(bEditMode)
	{
		SelectedTaskClass = SelectedTaskListItems.Num() > 0 ? SelectedTaskListItems[0]->Task->GetClass() : nullptr;
	}
	else if(SelectedTaskClass == nullptr)
	{
		SelectedTaskClass = UTaskBase::StaticClass();
	}
	GConfig->SetBool(TEXT("/Script/WHFrameworkEditor.TaskEditorSettings"), TEXT("bEditMode"), bEditMode, GTaskEditorIni);
}

void STaskListWidget::ToggleMultiMode()
{
	SetIsMultiMode(!bMultiMode);
}

void STaskListWidget::SetIsMultiMode(bool bIsMultiMode)
{
	bMultiMode = bIsMultiMode;
	if(!bMultiMode)
	{
		TreeView->ClearSelection();
	}
	GConfig->SetBool(TEXT("/Script/WHFrameworkEditor.TaskEditorSettings"), TEXT("bMultiMode"), bMultiMode, GTaskEditorIni);
}

int32 STaskListWidget::GetTotalTaskNum() const
{
	int32 TotalNum = 0;
	for(auto Iter : TaskListItems)
	{
		TotalNum++;
		Iter->GetSubTaskNum(TotalNum);
	}
	return TotalNum;
}

int32 STaskListWidget::GetSelectedTaskNum() const
{
	return SelectedTaskListItems.Num();
}

void STaskListWidget::UpdateTreeView(bool bRegenerate)
{
	if(!TaskModule) return;

	if(bRegenerate)
	{
		TaskModule->GenerateListItem(TaskListItems);
		for(auto Iter : TaskListItems)
		{
			SetTreeItemExpansionRecursive(Iter);
		}
	}
	else
	{
		TaskModule->UpdateListItem(TaskListItems);
	}
	TreeView->ClearSelection();
	TreeView->RequestTreeRefresh();
}

void STaskListWidget::UpdateSelection()
{
	SelectedTaskListItems = TreeView->GetSelectedItems();
	
	if(bEditMode)
	{
		SelectedTaskClass = SelectedTaskListItems.Num() > 0 ? SelectedTaskListItems[0]->Task->GetClass() : nullptr;
	}

	if(OnSelectTaskListItemsDelegate.IsBound())
	{
		OnSelectTaskListItemsDelegate.Execute(SelectedTaskListItems);
	}
}

TSharedRef<ITableRow> STaskListWidget::GenerateTreeRow(TSharedPtr<FTaskListItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(TreeItem.IsValid());

	return SNew(STableRow<TSharedPtr<FTaskListItem>>, OwnerTable)
	[
		SNew(STaskListItemWidget, TreeItem)
	];
}

void STaskListWidget::TreeItemScrolledIntoView(TSharedPtr<FTaskListItem> TreeItem, const TSharedPtr<ITableRow>& Widget) { }

void STaskListWidget::GetChildrenForTree(TSharedPtr<FTaskListItem> TreeItem, TArray<TSharedPtr<FTaskListItem>>& OutChildren)
{
	OutChildren = TreeItem->SubListItems;
}

void STaskListWidget::OnTreeItemExpansionChanged(TSharedPtr<FTaskListItem> TreeItem, bool bInExpansionState)
{
	if(TreeItem->GetStates().bExpanded != bInExpansionState)
	{
		TreeItem->GetStates().bExpanded = bInExpansionState;

		TreeItem->Task->Modify();
	}
}

void STaskListWidget::SetTreeItemExpansionRecursive(TSharedPtr<FTaskListItem> TreeItem)
{
	TreeView->SetItemExpansion(TreeItem, TreeItem->GetStates().bExpanded);

	for(auto Iter : TreeItem->SubListItems)
	{
		SetTreeItemExpansionRecursive(Iter);
	}
}

void STaskListWidget::SetTreeItemExpansionRecursive(TSharedPtr<FTaskListItem> TreeItem, bool bInExpansionState)
{
	TreeView->SetItemExpansion(TreeItem, bInExpansionState);

	for(auto Iter : TreeItem->SubListItems)
	{
		SetTreeItemExpansionRecursive(Iter, bInExpansionState);
	}
}

void STaskListWidget::TreeSelectionChanged(TSharedPtr<FTaskListItem> TreeItem, ESelectInfo::Type SelectInfo)
{
	UpdateSelection();
}

FReply STaskListWidget::OnEditTaskItemButtonClicked()
{
	if(!TaskModule) return FReply::Handled();

	if(SelectedTaskClass->ClassGeneratedBy)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(SelectedTaskClass->ClassGeneratedBy);
	}
	else
	{
		FSourceCodeNavigation::NavigateToClass(SelectedTaskClass);
	}
	return FReply::Handled();
}

FReply STaskListWidget::OnNewTaskClassButtonClicked()
{
	IAssetTools* AssetTools = &FModuleManager::GetModuleChecked<FAssetToolsModule>(FName("AssetTools")).Get();

	FString DefaultAssetPath = TEXT("/Game");
	
	GConfig->GetString(TEXT("/Script/WHFrameworkEditor.TaskEditorSettings"), TEXT("DefaultAssetPath"), DefaultAssetPath, GTaskEditorIni);

	UTaskBlueprintFactory* TaskBlueprintFactory = NewObject<UTaskBlueprintFactory>(GetTransientPackage(), UTaskBlueprintFactory::StaticClass());

	const TSharedRef<SCreateBlueprintAssetDialog> CreateBlueprintAssetDialog = SNew(SCreateBlueprintAssetDialog)
	.DefaultAssetPath(FText::FromString(DefaultAssetPath))
	.BlueprintFactory(TaskBlueprintFactory);

	if(CreateBlueprintAssetDialog->ShowModal() != EAppReturnType::Cancel)
	{
		DefaultAssetPath = CreateBlueprintAssetDialog->GetAssetPath();

		const FString AssetName = CreateBlueprintAssetDialog->GetAssetName();
	
		const FString PackageName = CreateBlueprintAssetDialog->GetPackageName();

		GConfig->SetString(TEXT("/Script/WHFrameworkEditor.TaskEditorSettings"), TEXT("DefaultAssetPath"), *DefaultAssetPath, GTaskEditorIni);
	
		if(UTaskBlueprint* TaskBlueprintAsset = Cast<UTaskBlueprint>(AssetTools->CreateAsset(AssetName, DefaultAssetPath, UTaskBlueprint::StaticClass(), TaskBlueprintFactory, FName("ContentBrowserNewAsset"))))
		{
			TArray<UObject*> ObjectsToSyncTo;
			ObjectsToSyncTo.Add(TaskBlueprintAsset);

			GEditor->SyncBrowserToObjects(ObjectsToSyncTo);

			//SelectedTaskClass = TaskBlueprintAsset->GetClass();
		}
	}

	return FReply::Handled();
}

FReply STaskListWidget::OnAddTaskItemButtonClicked()
{
	if(!TaskModule) return FReply::Handled();

	UTaskBase* NewTask = GenerateTask(SelectedTaskClass);

	const auto Item = MakeShared<FTaskListItem>();
	Item->Task = NewTask;

	TaskModule->GetTaskMap().Add(NewTask->TaskGUID, NewTask);

	if(SelectedTaskListItems.Num() > 0)
	{
		Item->ParentListItem = SelectedTaskListItems[0];

		NewTask->TaskIndex = SelectedTaskListItems[0]->GetSubTasks().Num();
		NewTask->TaskHierarchy = SelectedTaskListItems[0]->Task->TaskHierarchy + 1;
		SelectedTaskListItems[0]->GetSubTasks().Add(NewTask);
		SelectedTaskListItems[0]->SubListItems.Add(Item);

		SelectedTaskListItems[0]->Task->Modify();
	}
	else
	{
		NewTask->TaskIndex = TaskModule->GetRootTasks().Num();
		NewTask->TaskHierarchy = 0;
		TaskModule->GetRootTasks().Add(NewTask);
		TaskListItems.Add(Item);

		TaskModule->Modify();
	}

	if(Item->ParentListItem)
	{
		TreeView->SetItemExpansion(Item->ParentListItem, true);
	}
	TreeView->SetSelection(Item);
	UpdateTreeView();

	return FReply::Handled();
}

FReply STaskListWidget::OnInsertTaskItemButtonClicked()
{
	if(!TaskModule) return FReply::Handled();

	if(SelectedTaskListItems.Num() > 0)
	{
		UTaskBase* NewTask = GenerateTask(SelectedTaskClass);

		const auto Item = MakeShared<FTaskListItem>();
		Item->Task = NewTask;
		Item->ParentListItem = SelectedTaskListItems[0]->ParentListItem;

		TaskModule->GetTaskMap().Add(NewTask->TaskGUID, NewTask);

		if(SelectedTaskListItems[0]->GetParentTask())
		{
			SelectedTaskListItems[0]->GetParentSubTasks().Insert(NewTask, SelectedTaskListItems[0]->Task->TaskIndex);
			SelectedTaskListItems[0]->GetParentSubListItems().Insert(Item, SelectedTaskListItems[0]->Task->TaskIndex);

			SelectedTaskListItems[0]->GetParentTask()->Modify();
		}
		else
		{
			TaskModule->GetRootTasks().Insert(NewTask, SelectedTaskListItems[0]->Task->TaskIndex);
			TaskListItems.Insert(Item, SelectedTaskListItems[0]->Task->TaskIndex);

			TaskModule->Modify();
		}

		TreeView->SetSelection(Item);
		UpdateTreeView();
	}

	return FReply::Handled();
}

FReply STaskListWidget::OnAppendTaskItemButtonClicked()
{
	if(!TaskModule) return FReply::Handled();

	if(SelectedTaskListItems.Num() > 0)
	{
		UTaskBase* NewTask = GenerateTask(SelectedTaskClass);

		const auto Item = MakeShared<FTaskListItem>();
		Item->Task = NewTask;
		Item->ParentListItem = SelectedTaskListItems[0]->ParentListItem;

		TaskModule->GetTaskMap().Add(NewTask->TaskGUID, NewTask);

		if(SelectedTaskListItems[0]->GetParentTask())
		{
			SelectedTaskListItems[0]->GetParentSubTasks().Insert(NewTask, SelectedTaskListItems[0]->Task->TaskIndex + 1);
			SelectedTaskListItems[0]->GetParentSubListItems().Insert(Item, SelectedTaskListItems[0]->Task->TaskIndex + 1);

			SelectedTaskListItems[0]->GetParentTask()->Modify();
		}
		else
		{
			TaskModule->GetRootTasks().Insert(NewTask, SelectedTaskListItems[0]->Task->TaskIndex + 1);
			TaskListItems.Insert(Item, SelectedTaskListItems[0]->Task->TaskIndex + 1);

			TaskModule->Modify();
		}

		TreeView->SetSelection(Item);
		UpdateTreeView();
	}

	return FReply::Handled();
}

FReply STaskListWidget::OnCopyTaskItemButtonClicked()
{
	if(!TaskModule) return FReply::Handled();

	CopiedTask = DuplicateTask(SelectedTaskListItems[0]->Task);

	return FReply::Handled();
}

FReply STaskListWidget::OnPasteTaskItemButtonClicked()
{
	if(!TaskModule || !CopiedTask) return FReply::Handled();

	const auto Item = MakeShared<FTaskListItem>();
	Item->Task = CopiedTask;

	TaskModule->GetTaskMap().Add(CopiedTask->TaskGUID, CopiedTask);

	if(SelectedTaskListItems.Num() > 0)
	{
		Item->ParentListItem = SelectedTaskListItems[0];

		CopiedTask->TaskIndex = SelectedTaskListItems[0]->GetSubTasks().Num();
		CopiedTask->TaskHierarchy = SelectedTaskListItems[0]->Task->TaskHierarchy;
		SelectedTaskListItems[0]->GetSubTasks().Add(CopiedTask);
		SelectedTaskListItems[0]->SubListItems.Add(Item);

		SelectedTaskListItems[0]->Task->Modify();
	}
	else
	{
		CopiedTask->TaskIndex = TaskModule->GetRootTasks().Num();
		CopiedTask->TaskHierarchy = 0;
		TaskModule->GetRootTasks().Add(CopiedTask);
		TaskListItems.Add(Item);

		TaskModule->Modify();
	}

	if(Item->ParentListItem)
	{
		TreeView->SetItemExpansion(Item->ParentListItem, true);
	}
	TreeView->SetSelection(Item);
	UpdateTreeView();

	CopiedTask = nullptr;

	return FReply::Handled();
}

FReply STaskListWidget::OnDuplicateTaskItemButtonClicked()
{
	if(!TaskModule) return FReply::Handled();

	if(SelectedTaskListItems.Num() > 0)
	{
		UTaskBase* NewTask = DuplicateTask(SelectedTaskListItems[0]->Task);

		const auto Item = MakeShared<FTaskListItem>();
		Item->Task = NewTask;
		Item->ParentListItem = SelectedTaskListItems[0]->ParentListItem;

		TaskModule->GetTaskMap().Add(NewTask->TaskGUID, NewTask);

		if(SelectedTaskListItems[0]->GetParentTask())
		{
			SelectedTaskListItems[0]->GetParentSubTasks().Insert(NewTask, SelectedTaskListItems[0]->Task->TaskIndex + 1);
			SelectedTaskListItems[0]->GetParentSubListItems().Insert(Item, SelectedTaskListItems[0]->Task->TaskIndex + 1);

			SelectedTaskListItems[0]->GetParentTask()->Modify();
		}
		else
		{
			TaskModule->GetRootTasks().Insert(NewTask, SelectedTaskListItems[0]->Task->TaskIndex + 1);
			TaskListItems.Insert(Item, SelectedTaskListItems[0]->Task->TaskIndex + 1);

			TaskModule->Modify();
		}

		TreeView->SetSelection(Item);
		UpdateTreeView();
	}

	return FReply::Handled();
}

FReply STaskListWidget::OnExpandAllTaskItemButtonClicked()
{
	for(auto Iter : TaskListItems)
	{
		SetTreeItemExpansionRecursive(Iter, true);
	}

	return FReply::Handled();
}

FReply STaskListWidget::OnCollapseAllTaskItemButtonClicked()
{
	for(auto Iter : TaskListItems)
	{
		SetTreeItemExpansionRecursive(Iter, false);
	}

	return FReply::Handled();
}

FReply STaskListWidget::OnRemoveTaskItemButtonClicked()
{
	if(!TaskModule) return FReply::Handled();

	if(FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("Are you sure to remove selected Tasks?"))) != EAppReturnType::Yes) return FReply::Handled();

	if(SelectedTaskListItems.Num() > 0)
	{
		for(auto Iter : SelectedTaskListItems)
		{
			TaskModule->GetTaskMap().Remove(Iter->Task->TaskGUID);

			if(Iter->GetParentTask())
			{
				Iter->GetParentSubTasks()[Iter->GetTaskIndex()]->OnUnGenerate();
				Iter->GetParentSubTasks().RemoveAt(Iter->GetTaskIndex());
				Iter->GetParentSubListItems().RemoveAt(Iter->GetTaskIndex());

				Iter->GetParentTask()->Modify();

				//TreeView->SetSelection(SelectedTaskListItem->GetParentSubListItems()[FMath::Min(SelectedTaskListItem->GetTaskIndex(), SelectedTaskListItem->GetParentSubListItems().Num() - 1)]);
			}
			else
			{
				TaskModule->GetRootTasks()[Iter->GetTaskIndex()]->OnUnGenerate();
				TaskModule->GetRootTasks().RemoveAt(Iter->GetTaskIndex());
				TaskListItems.RemoveAt(Iter->GetTaskIndex());
				//TreeView->SetSelection(TaskListItems[FMath::Min(SelectedTaskListItem->GetTaskIndex(),TaskListItems.Num() - 1)]);

				TaskModule->Modify();
			}
		}
		UpdateTreeView();
	}

	return FReply::Handled();
}

FReply STaskListWidget::OnClearAllTaskItemButtonClicked()
{
	if(!TaskModule) return FReply::Handled();

	if(FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("Are you sure to clear all tasks?"))) != EAppReturnType::Yes) return FReply::Handled();

	TaskModule->ClearAllTask();

	UpdateTreeView(true);

	return FReply::Handled();
}

FReply STaskListWidget::OnMoveUpTaskItemButtonClicked()
{
	if(!TaskModule || SelectedTaskListItems.Num() == 0 || SelectedTaskListItems[0]->GetTaskIndex() == 0) return FReply::Handled();

	if(SelectedTaskListItems[0]->GetParentTask())
	{
		const auto TmpTask = SelectedTaskListItems[0]->GetParentSubTasks()[SelectedTaskListItems[0]->GetTaskIndex()];
		SelectedTaskListItems[0]->GetParentSubTasks().RemoveAt(SelectedTaskListItems[0]->GetTaskIndex());
		SelectedTaskListItems[0]->GetParentSubTasks().Insert(TmpTask, SelectedTaskListItems[0]->GetTaskIndex() - 1);

		SelectedTaskListItems[0]->GetParentSubListItems().RemoveAt(SelectedTaskListItems[0]->GetTaskIndex());
		SelectedTaskListItems[0]->GetParentSubListItems().Insert(SelectedTaskListItems, SelectedTaskListItems[0]->GetTaskIndex() - 1);

		SelectedTaskListItems[0]->GetParentTask()->Modify();

		UpdateTreeView();
	}
	else
	{
		const auto TmpTask = TaskModule->GetRootTasks()[SelectedTaskListItems[0]->GetTaskIndex()];
		TaskModule->GetRootTasks().RemoveAt(SelectedTaskListItems[0]->GetTaskIndex());
		TaskModule->GetRootTasks().Insert(TmpTask, SelectedTaskListItems[0]->GetTaskIndex() - 1);

		TaskListItems.RemoveAt(SelectedTaskListItems[0]->GetTaskIndex());
		TaskListItems.Insert(SelectedTaskListItems, SelectedTaskListItems[0]->GetTaskIndex() - 1);

		TaskModule->Modify();

		UpdateTreeView();
	}

	return FReply::Handled();
}

FReply STaskListWidget::OnMoveDownTaskItemButtonClicked()
{
	if(!TaskModule || SelectedTaskListItems.Num() == 0) return FReply::Handled();

	if(SelectedTaskListItems[0]->GetParentTask())
	{
		if(SelectedTaskListItems[0]->GetTaskIndex() < SelectedTaskListItems[0]->GetParentSubTasks().Num() - 1)
		{
			const auto TmpTask = SelectedTaskListItems[0]->GetParentSubTasks()[SelectedTaskListItems[0]->GetTaskIndex()];
			SelectedTaskListItems[0]->GetParentSubTasks().RemoveAt(SelectedTaskListItems[0]->GetTaskIndex());
			SelectedTaskListItems[0]->GetParentSubTasks().Insert(TmpTask, SelectedTaskListItems[0]->GetTaskIndex() + 1);

			SelectedTaskListItems[0]->GetParentSubListItems().RemoveAt(SelectedTaskListItems[0]->GetTaskIndex());
			SelectedTaskListItems[0]->GetParentSubListItems().Insert(SelectedTaskListItems, SelectedTaskListItems[0]->GetTaskIndex() + 1);

			SelectedTaskListItems[0]->GetParentTask()->Modify();

			UpdateTreeView();
		}
	}
	else
	{
		if(SelectedTaskListItems[0]->GetTaskIndex() < TaskModule->GetRootTasks().Num() - 1)
		{
			const auto TmpTask = TaskModule->GetRootTasks()[SelectedTaskListItems[0]->GetTaskIndex()];
			TaskModule->GetRootTasks().RemoveAt(SelectedTaskListItems[0]->GetTaskIndex());
			TaskModule->GetRootTasks().Insert(TmpTask, SelectedTaskListItems[0]->GetTaskIndex() + 1);

			TaskListItems.RemoveAt(SelectedTaskListItems[0]->GetTaskIndex());
			TaskListItems.Insert(SelectedTaskListItems, SelectedTaskListItems[0]->GetTaskIndex() + 1);

			TaskModule->Modify();

			UpdateTreeView();
		}
	}

	return FReply::Handled();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
