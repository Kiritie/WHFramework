// Fill out your copyright notice in the Description page of Project Settings.

#include "Step/Slate/SStepListWidget.h"

#include "SlateOptMacros.h"
#include "SourceCodeNavigation.h"
#include "WHFrameworkCoreStatics.h"
#include "WHFrameworkEditorStatics.h"
#include "Step/StepEditorTypes.h"
#include "Step/Base/StepAsset.h"
#include "Step/Blueprint/StepBlueprintFactory.h"
#include "Step/Slate/SStepDetailsWidget.h"
#include "Widgets/Input/SSearchBox.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

FName SStepListWidget::WidgetName = FName("StepListWidget");

SStepListWidget::SStepListWidget()
{
	WidgetType = EEditorWidgetType::Child;

	bDefaults = false;
	bEditing = false;

	CopiedStep = nullptr;
}

void SStepListWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());
	
	StepEditor = InArgs._StepEditor;

	if(!TreeView.IsValid())
	{
		SAssignNew(TreeView, STreeView< TSharedPtr<FStepListItem> >)
			.TreeItemsSource(&VisibleStepListItems)
			.OnGenerateRow(this, &SStepListWidget::GenerateTreeRow)
			.OnItemScrolledIntoView(this, &SStepListWidget::TreeItemScrolledIntoView)
			.ItemHeight(18)
			.SelectionMode(ESelectionMode::Multi)
			.OnSelectionChanged(this, &SStepListWidget::TreeSelectionChanged)
			.OnGetChildren(this, &SStepListWidget::GetChildrenForTree)
			.OnExpansionChanged(this, &SStepListWidget::TreeItemExpansionChanged)
			.OnSetExpansionRecursive(this, &SStepListWidget::SetTreeItemExpansionRecursive)
			.ClearSelectionOnClick(true)
			.HighlightParentNodesForSelection(true);
	}

	if(!ScrollBar.IsValid())
	{
		SAssignNew(ScrollBar, SScrollBar)
			.Thickness(FVector2D(9.0f, 9.0f));
	}

	SelectedStepClass = UStepBase::StaticClass();

	ClassViewerOptions.bShowBackgroundBorder = false;
	ClassViewerOptions.bShowUnloadedBlueprints = true;
	ClassViewerOptions.bShowNoneOption = false;

	ClassViewerOptions.bIsBlueprintBaseOnly = false;
	ClassViewerOptions.bIsPlaceableOnly = false;
	ClassViewerOptions.NameTypeToDisplay = EClassViewerNameTypeToDisplay::ClassName;
	ClassViewerOptions.DisplayMode = EClassViewerDisplayMode::ListView;
	ClassViewerOptions.bAllowViewOptions = true;

	StepClassFilter = MakeShareable(new FStepClassFilter);
	StepClassFilter->StepEditor = StepEditor;
	
	ClassViewerOptions.ClassFilters.Add(StepClassFilter.ToSharedRef());

	SAssignNew(ClassPickButton, SComboButton)
		.OnGetMenuContent(this, &SStepListWidget::GenerateClassPicker)
		.ContentPadding(FMargin(2.0f, 2.0f))
		.ToolTipText(this, &SStepListWidget::GetPickedClassName)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &SStepListWidget::GetPickedClassName)
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
						.OnClicked(this, &SStepListWidget::OnNewStepClassButtonClicked)
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
						.IsEnabled_Lambda([this](){ return SelectedStepClass != nullptr; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SStepListWidget::OnEditStepItemButtonClicked)
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
						.IsEnabled_Lambda([this](){ return ActiveFilterText.IsEmptyOrWhitespace() ? (!bEditing && SelectedStepListItems.Num() <= 1 && SelectedStepClass) : false; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SStepListWidget::OnAddStepItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Insert")))
						.IsEnabled_Lambda([this](){ return ActiveFilterText.IsEmptyOrWhitespace() ? (!bEditing && SelectedStepListItems.Num() == 1 && SelectedStepClass) : false; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SStepListWidget::OnInsertStepItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Append")))
						.IsEnabled_Lambda([this](){ return ActiveFilterText.IsEmptyOrWhitespace() ? (!bEditing && SelectedStepListItems.Num() == 1 && SelectedStepClass) : false; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SStepListWidget::OnAppendStepItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Copy")))
						.IsEnabled_Lambda([this](){ return ActiveFilterText.IsEmptyOrWhitespace() ? (SelectedStepListItems.Num() == 1) : false; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SStepListWidget::OnCopyStepItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Paste")))
						.IsEnabled_Lambda([this](){ return ActiveFilterText.IsEmptyOrWhitespace() ? (CopiedStep != nullptr) : false; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SStepListWidget::OnPasteStepItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Duplicate")))
						.IsEnabled_Lambda([this](){ return ActiveFilterText.IsEmptyOrWhitespace() ? (SelectedStepListItems.Num() == 1) : false; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SStepListWidget::OnDuplicateStepItemButtonClicked)
					]
				]
				
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(1.f, 4.f)
					[
						SAssignNew(SearchBox, SSearchBox)
						.InitialText(this, &SStepListWidget::GetFilterText)
						.OnTextChanged(this, &SStepListWidget::OnFilterTextChanged)
						.OnTextCommitted(this, &SStepListWidget::OnFilterTextCommitted)
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
						.IsEnabled_Lambda([this](){ return StepEditor.Pin()->GetEditingAsset<UStepAsset>()->GetRootSteps().Num() > 0; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SStepListWidget::OnExpandAllStepItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Collapse")))
						.IsEnabled_Lambda([this](){ return StepEditor.Pin()->GetEditingAsset<UStepAsset>()->GetRootSteps().Num() > 0; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SStepListWidget::OnCollapseAllStepItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Clear All")))
						.IsEnabled_Lambda([this](){ return StepEditor.Pin()->GetEditingAsset<UStepAsset>()->GetRootSteps().Num() > 0; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SStepListWidget::OnClearAllStepItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Remove")))
						.IsEnabled_Lambda([this](){ return ActiveFilterText.IsEmptyOrWhitespace() ? (SelectedStepListItems.Num() > 0) : false; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SStepListWidget::OnRemoveStepItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Move Up")))
						.IsEnabled_Lambda([this](){ return ActiveFilterText.IsEmptyOrWhitespace() ? (SelectedStepListItems.Num() == 1 && SelectedStepListItems[0]->GetStepIndex() > 0) : false; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SStepListWidget::OnMoveUpStepItemButtonClicked)
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
							return ActiveFilterText.IsEmptyOrWhitespace() ? (SelectedStepListItems.Num() == 1 &&
								SelectedStepListItems[0]->GetStepIndex() < (SelectedStepListItems[0]->GetParentStep() ? SelectedStepListItems[0]->GetParentSubSteps().Num() : StepEditor.Pin()->GetEditingAsset<UStepAsset>()->GetRootSteps().Num()) - 1) : false;
						})
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SStepListWidget::OnMoveDownStepItemButtonClicked)
					]
				]
			]
		]
	];

	GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.StepEditorSettings"), TEXT("bDefaults"), bDefaults, GStepEditorIni);
	GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.StepEditorSettings"), TEXT("bEditing"), bEditing, GStepEditorIni);
}

void SStepListWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();
}

void SStepListWidget::OnInitialize()
{
	SEditorWidgetBase::OnInitialize();

	SetIsDefaults(bDefaults);
	SetIsEditing(bEditing);

	UpdateTreeView(true);
}

void SStepListWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void SStepListWidget::OnRefresh()
{
	SEditorWidgetBase::OnRefresh();

	UpdateTreeView(true);
	UpdateSelection();
	SetIsEditing(bEditing);
}

void SStepListWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

FReply SStepListWidget::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if(InKeyEvent.GetKey() == EKeys::Escape)
	{
		TreeView->ClearSelection();
		return FReply::Handled();
	}
	return SEditorWidgetBase::OnKeyDown(MyGeometry, InKeyEvent);
}

UStepBase* SStepListWidget::GenerateStep(TSubclassOf<UStepBase> InClass)
{
	UStepBase* NewStep = NewObject<UStepBase>(StepEditor.Pin()->GetEditingAsset<UStepAsset>(), InClass, NAME_None);

	// NewStep->StepName = *CurrentStepClass->GetName().Replace(TEXT("Step_"), TEXT(""));
	// NewStep->StepDisplayName = FText::FromName(NewStep->StepName);

	NewStep->OnGenerate();

	return NewStep;
}

UStepBase* SStepListWidget::DuplicateStep(UStepBase* InStep)
{
	UStepBase* NewStep = DuplicateObject<UStepBase>(InStep, StepEditor.Pin()->GetEditingAsset<UStepAsset>(), NAME_None);

	NewStep->StepGUID = FGuid::NewGuid().ToString();
	NewStep->SubSteps.Empty();

	// NewStep->StepName = *FString::Printf(TEXT("%s_Copy"), *NewStep->StepName.ToString());
	// NewStep->StepDisplayName = FText::FromString(FString::Printf(TEXT("%s_Copy"), *NewStep->StepDisplayName.ToString()));

	NewStep->OnGenerate();

	return NewStep;
}

TSharedRef<SWidget> SStepListWidget::GenerateClassPicker()
{
	FOnClassPicked OnPicked(FOnClassPicked::CreateRaw(this, &SStepListWidget::OnClassPicked));

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

void SStepListWidget::OnClassPicked(UClass* InClass)
{
	ClassPickButton->SetIsOpen(false);

	if(!bEditing)
	{
		SelectedStepClass = InClass;
	}
	else if(SelectedStepListItems.Num() == 1)
	{
		if(FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("Are you sure to change selected step class?"))) == EAppReturnType::Yes)
		{
			UStepBase* OldStep = SelectedStepListItems[0]->Step;
			UStepBase* NewStep = GenerateStep(InClass);

			if(NewStep && OldStep)
			{
				FCoreStatics::ExportPropertiesToObject(OldStep, NewStep);
				if(OldStep->ParentStep)
				{
					OldStep->ParentStep->SubSteps[OldStep->StepIndex] = NewStep;
				}
				else
				{
					StepEditor.Pin()->GetEditingAsset<UStepAsset>()->GetRootSteps()[OldStep->StepIndex] = NewStep;
				}
				OldStep->OnUnGenerate();
			}

			StepEditor.Pin()->GetEditingAsset<UStepAsset>()->Modify();

			UpdateTreeView();

			UpdateSelection();

			SelectedStepClass = InClass;
		}
	}
}

FText SStepListWidget::GetPickedClassName() const
{
	FString ClassName = SelectedStepClass ? SelectedStepClass->GetName() : TEXT("None");
	ClassName.RemoveFromEnd(TEXT("_C"));
	return FText::FromString(ClassName);
}

FText SStepListWidget::GetFilterText() const
{
	return ActiveFilterText;
}

void SStepListWidget::OnFilterTextChanged(const FText& InFilterText)
{
	ActiveFilterText = InFilterText;

	UpdateTreeView();
}

void SStepListWidget::OnFilterTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnCleared)
	{
		SearchBox->SetText(FText::GetEmpty());
		OnFilterTextChanged(FText::GetEmpty());
	}
}

void SStepListWidget::ToggleEditing()
{
	SetIsEditing(!bEditing);
}

void SStepListWidget::SetIsEditing(bool bIsEditing)
{
	bEditing = bIsEditing;
	if(bEditing)
	{
		SelectedStepClass = SelectedStepListItems.Num() > 0 ? SelectedStepListItems[0]->Step->GetClass() : nullptr;
	}
	else if(SelectedStepClass == nullptr)
	{
		SelectedStepClass = UStepBase::StaticClass();
	}
	GConfig->SetBool(TEXT("/Script/WHFrameworkEditor.StepEditorSettings"), TEXT("bEditing"), bEditing, GStepEditorIni);
}

void SStepListWidget::ToggleDefaults()
{
	SetIsDefaults(!bDefaults);
}

void SStepListWidget::SetIsDefaults(bool bIsDefaults)
{
	bDefaults = bIsDefaults;
	GConfig->SetBool(TEXT("/Script/WHFrameworkEditor.StepEditorSettings"), TEXT("bDefaults"), bDefaults, GStepEditorIni);

	StepEditor.Pin()->DetailsWidget->Refresh();
}

int32 SStepListWidget::GetTotalStepNum() const
{
	int32 TotalNum = 0;
	for(auto Iter : StepListItems)
	{
		TotalNum++;
		Iter->GetSubStepNum(TotalNum);
	}
	return TotalNum;
}

int32 SStepListWidget::GetSelectedStepNum() const
{
	return SelectedStepListItems.Num();
}

void SStepListWidget::UpdateTreeView(bool bRegenerate)
{
	if(!StepEditor.Pin()->GetEditingAsset<UStepAsset>()) return;

	ClearSelection();

	if(ActiveFilterText.IsEmptyOrWhitespace())
	{
		if(bRegenerate)
		{
			StepEditor.Pin()->GetEditingAsset<UStepAsset>()->GenerateStepListItem(StepListItems);
		}
		else
		{
			StepEditor.Pin()->GetEditingAsset<UStepAsset>()->UpdateStepListItem(StepListItems);
		}
		VisibleStepListItems = StepListItems;
	}
	else
	{
		StepEditor.Pin()->GetEditingAsset<UStepAsset>()->GenerateStepListItem(VisibleStepListItems, ActiveFilterText.ToString());
	}
	for(auto Iter : VisibleStepListItems)
	{
		SetTreeItemExpansionRecursive(Iter);
		SetTreeItemSelectionRecursive(Iter);
	}

	TreeView->RequestTreeRefresh();
}

void SStepListWidget::UpdateSelection()
{
	SelectedStepListItems = TreeView->GetSelectedItems();

	if(bEditing)
	{
		SelectedStepClass = SelectedStepListItems.Num() > 0 ? SelectedStepListItems[0]->Step->GetClass() : nullptr;
	}

	if(OnSelectStepListItemsDelegate.IsBound())
	{
		OnSelectStepListItemsDelegate.Execute(SelectedStepListItems);
	}

	StepEditor.Pin()->DetailsWidget->Refresh();
}

void SStepListWidget::ClearSelection()
{
	TreeView->ClearSelection();
}

TSharedRef<ITableRow> SStepListWidget::GenerateTreeRow(TSharedPtr<FStepListItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(TreeItem.IsValid());

	return SNew(SStepListItemWidget, OwnerTable)
		.Item(TreeItem)
		.ListWidget(SharedThis(this));
}

void SStepListWidget::TreeItemScrolledIntoView(TSharedPtr<FStepListItem> TreeItem, const TSharedPtr<ITableRow>& Widget) { }

void SStepListWidget::GetChildrenForTree(TSharedPtr<FStepListItem> TreeItem, TArray<TSharedPtr<FStepListItem>>& OutChildren)
{
	OutChildren = TreeItem->SubListItems;
}

void SStepListWidget::SetTreeItemSelectionRecursive(TSharedPtr<FStepListItem> TreeItem)
{
	TreeView->SetItemSelection(TreeItem, TreeItem->GetStates().bSelected);

	for(auto Iter : TreeItem->SubListItems)
	{
		SetTreeItemSelectionRecursive(Iter);
	}
}

void SStepListWidget::TreeItemExpansionChanged(TSharedPtr<FStepListItem> TreeItem, bool bInExpansionState)
{
	TreeItem->GetStates().bExpanded = bInExpansionState;
}

void SStepListWidget::SetTreeItemExpansionRecursive(TSharedPtr<FStepListItem> TreeItem)
{
	TreeView->SetItemExpansion(TreeItem, TreeItem->GetStates().bExpanded);

	for(auto Iter : TreeItem->SubListItems)
	{
		SetTreeItemExpansionRecursive(Iter);
	}
}

void SStepListWidget::SetTreeItemExpansionRecursive(TSharedPtr<FStepListItem> TreeItem, bool bInExpansionState)
{
	TreeView->SetItemExpansion(TreeItem, bInExpansionState);

	for(auto Iter : TreeItem->SubListItems)
	{
		SetTreeItemExpansionRecursive(Iter, bInExpansionState);
	}
}

void SStepListWidget::TreeSelectionChanged(TSharedPtr<FStepListItem> TreeItem, ESelectInfo::Type SelectInfo)
{
	if(SelectInfo != ESelectInfo::Direct)
	{
		if(!ActiveFilterText.IsEmptyOrWhitespace())
		{
			for(auto Iter : StepListItems)
			{
				RecursiveArrayItems<TSharedPtr<FStepListItem>>(Iter, [this](const TSharedPtr<FStepListItem>& Item)
				{
					Item->GetStates().bSelected = false;
					return Item->SubListItems;
				});
			}
		}
		for(auto Iter : VisibleStepListItems)
		{
			RecursiveArrayItems<TSharedPtr<FStepListItem>>(Iter, [this](const TSharedPtr<FStepListItem>& Item)
			{
				Item->GetStates().bSelected = TreeView->IsItemSelected(Item);
				return Item->SubListItems;
			});
		}
	}
	UpdateSelection();
}

FReply SStepListWidget::OnEditStepItemButtonClicked()
{
	if(!StepEditor.Pin()->GetEditingAsset<UStepAsset>()) return FReply::Handled();

	if(SelectedStepClass->ClassGeneratedBy)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(SelectedStepClass->ClassGeneratedBy);
	}
	else
	{
		FSourceCodeNavigation::NavigateToClass(SelectedStepClass);
	}
	return FReply::Handled();
}

FReply SStepListWidget::OnNewStepClassButtonClicked()
{
	if(const UBlueprint* Blueprint = UWHFrameworkEditorStatics::CreateBlueprintAssetWithDialog(UStepBlueprintFactory::StaticClass()))
	{
		if(!bEditing)
		{
			SelectedStepClass = Blueprint->GeneratedClass;
		}
	}

	return FReply::Handled();
}

FReply SStepListWidget::OnAddStepItemButtonClicked()
{
	if(!StepEditor.Pin()->GetEditingAsset<UStepAsset>()) return FReply::Handled();

	UStepBase* NewStep = GenerateStep(SelectedStepClass);

	const auto Item = MakeShared<FStepListItem>();
	Item->Step = NewStep;

	if(SelectedStepListItems.Num() > 0)
	{
		Item->ParentListItem = SelectedStepListItems[0];

		NewStep->StepIndex = SelectedStepListItems[0]->GetSubSteps().Num();
		NewStep->StepHierarchy = SelectedStepListItems[0]->Step->StepHierarchy + 1;
		SelectedStepListItems[0]->GetSubSteps().Add(NewStep);
		SelectedStepListItems[0]->SubListItems.Add(Item);

		SelectedStepListItems[0]->Step->Modify();
	}
	else
	{
		NewStep->StepIndex = StepEditor.Pin()->GetEditingAsset<UStepAsset>()->GetRootSteps().Num();
		NewStep->StepHierarchy = 0;
		StepEditor.Pin()->GetEditingAsset<UStepAsset>()->GetRootSteps().Add(NewStep);
		StepListItems.Add(Item);

		StepEditor.Pin()->GetEditingAsset<UStepAsset>()->Modify();
	}

	if(Item->ParentListItem)
	{
		TreeView->SetItemExpansion(Item->ParentListItem, true);
	}
	TreeView->SetSelection(Item);
	UpdateTreeView();

	return FReply::Handled();
}

FReply SStepListWidget::OnInsertStepItemButtonClicked()
{
	if(!StepEditor.Pin()->GetEditingAsset<UStepAsset>()) return FReply::Handled();

	if(SelectedStepListItems.Num() > 0)
	{
		UStepBase* NewStep = GenerateStep(SelectedStepClass);

		const auto Item = MakeShared<FStepListItem>();
		Item->Step = NewStep;
		Item->ParentListItem = SelectedStepListItems[0]->ParentListItem;

		if(SelectedStepListItems[0]->GetParentStep())
		{
			SelectedStepListItems[0]->GetParentSubSteps().Insert(NewStep, SelectedStepListItems[0]->Step->StepIndex);
			SelectedStepListItems[0]->GetParentSubListItems().Insert(Item, SelectedStepListItems[0]->Step->StepIndex);

			SelectedStepListItems[0]->GetParentStep()->Modify();
		}
		else if(UStepBase* NewRootStep = Cast<UStepBase>(NewStep))
		{
			StepEditor.Pin()->GetEditingAsset<UStepAsset>()->GetRootSteps().Insert(NewRootStep, SelectedStepListItems[0]->Step->StepIndex);
			StepListItems.Insert(Item, SelectedStepListItems[0]->Step->StepIndex);

			StepEditor.Pin()->GetEditingAsset<UStepAsset>()->Modify();
		}

		TreeView->SetSelection(Item);
		UpdateTreeView();
	}

	return FReply::Handled();
}

FReply SStepListWidget::OnAppendStepItemButtonClicked()
{
	if(!StepEditor.Pin()->GetEditingAsset<UStepAsset>()) return FReply::Handled();

	if(SelectedStepListItems.Num() > 0)
	{
		UStepBase* NewStep = GenerateStep(SelectedStepClass);

		const auto Item = MakeShared<FStepListItem>();
		Item->Step = NewStep;
		Item->ParentListItem = SelectedStepListItems[0]->ParentListItem;

		if(SelectedStepListItems[0]->GetParentStep())
		{
			SelectedStepListItems[0]->GetParentSubSteps().Insert(NewStep, SelectedStepListItems[0]->Step->StepIndex + 1);
			SelectedStepListItems[0]->GetParentSubListItems().Insert(Item, SelectedStepListItems[0]->Step->StepIndex + 1);

			SelectedStepListItems[0]->GetParentStep()->Modify();
		}
		else if(UStepBase* NewRootStep = Cast<UStepBase>(NewStep))
		{
			StepEditor.Pin()->GetEditingAsset<UStepAsset>()->GetRootSteps().Insert(NewRootStep, SelectedStepListItems[0]->Step->StepIndex + 1);
			StepListItems.Insert(Item, SelectedStepListItems[0]->Step->StepIndex + 1);

			StepEditor.Pin()->GetEditingAsset<UStepAsset>()->Modify();
		}

		TreeView->SetSelection(Item);
		UpdateTreeView();
	}

	return FReply::Handled();
}

FReply SStepListWidget::OnCopyStepItemButtonClicked()
{
	if(!StepEditor.Pin()->GetEditingAsset<UStepAsset>()) return FReply::Handled();

	CopiedStep = DuplicateStep(SelectedStepListItems[0]->Step);

	return FReply::Handled();
}

FReply SStepListWidget::OnPasteStepItemButtonClicked()
{
	if(!StepEditor.Pin()->GetEditingAsset<UStepAsset>() || !CopiedStep) return FReply::Handled();

	const auto Item = MakeShared<FStepListItem>();
	Item->Step = CopiedStep;

	if(SelectedStepListItems.Num() > 0)
	{
		Item->ParentListItem = SelectedStepListItems[0];

		CopiedStep->StepIndex = SelectedStepListItems[0]->GetSubSteps().Num();
		CopiedStep->StepHierarchy = SelectedStepListItems[0]->Step->StepHierarchy;
		SelectedStepListItems[0]->GetSubSteps().Add(CopiedStep);
		SelectedStepListItems[0]->SubListItems.Add(Item);

		SelectedStepListItems[0]->Step->Modify();
	}
	else
	{
		CopiedStep->StepIndex = StepEditor.Pin()->GetEditingAsset<UStepAsset>()->GetRootSteps().Num();
		CopiedStep->StepHierarchy = 0;
		StepEditor.Pin()->GetEditingAsset<UStepAsset>()->GetRootSteps().Add(CopiedStep);
		StepListItems.Add(Item);

		StepEditor.Pin()->GetEditingAsset<UStepAsset>()->Modify();
	}

	if(Item->ParentListItem)
	{
		TreeView->SetItemExpansion(Item->ParentListItem, true);
	}
	TreeView->SetSelection(Item);
	UpdateTreeView();

	CopiedStep = nullptr;

	return FReply::Handled();
}

FReply SStepListWidget::OnDuplicateStepItemButtonClicked()
{
	if(!StepEditor.Pin()->GetEditingAsset<UStepAsset>()) return FReply::Handled();

	if(SelectedStepListItems.Num() > 0)
	{
		UStepBase* NewStep = DuplicateStep(SelectedStepListItems[0]->Step);

		const auto Item = MakeShared<FStepListItem>();
		Item->Step = NewStep;
		Item->ParentListItem = SelectedStepListItems[0]->ParentListItem;

		if(SelectedStepListItems[0]->GetParentStep())
		{
			SelectedStepListItems[0]->GetParentSubSteps().Insert(NewStep, SelectedStepListItems[0]->Step->StepIndex + 1);
			SelectedStepListItems[0]->GetParentSubListItems().Insert(Item, SelectedStepListItems[0]->Step->StepIndex + 1);

			SelectedStepListItems[0]->GetParentStep()->Modify();
		}
		else if(UStepBase* NewRootStep = Cast<UStepBase>(NewStep))
		{
			StepEditor.Pin()->GetEditingAsset<UStepAsset>()->GetRootSteps().Insert(NewRootStep, SelectedStepListItems[0]->Step->StepIndex + 1);
			StepListItems.Insert(Item, SelectedStepListItems[0]->Step->StepIndex + 1);

			StepEditor.Pin()->GetEditingAsset<UStepAsset>()->Modify();
		}

		TreeView->SetSelection(Item);
		UpdateTreeView();
	}

	return FReply::Handled();
}

FReply SStepListWidget::OnExpandAllStepItemButtonClicked()
{
	for(auto Iter : StepListItems)
	{
		SetTreeItemExpansionRecursive(Iter, true);
	}

	return FReply::Handled();
}

FReply SStepListWidget::OnCollapseAllStepItemButtonClicked()
{
	for(auto Iter : StepListItems)
	{
		SetTreeItemExpansionRecursive(Iter, false);
	}

	return FReply::Handled();
}

FReply SStepListWidget::OnRemoveStepItemButtonClicked()
{
	if(!StepEditor.Pin()->GetEditingAsset<UStepAsset>()) return FReply::Handled();

	if(FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("Are you sure to remove selected steps?"))) != EAppReturnType::Yes) return FReply::Handled();

	if(SelectedStepListItems.Num() > 0)
	{
		for(auto Iter : SelectedStepListItems)
		{
			if(Iter->GetParentStep())
			{
				Iter->GetParentSubSteps()[Iter->GetStepIndex()]->OnUnGenerate();
				Iter->GetParentSubSteps().RemoveAt(Iter->GetStepIndex());
				Iter->GetParentSubListItems().RemoveAt(Iter->GetStepIndex());

				Iter->GetParentStep()->Modify();

				//TreeView->SetSelection(SelectedStepListItem->GetParentSubListItems()[FMath::Min(SelectedStepListItem->GetStepIndex(), SelectedStepListItem->GetParentSubListItems().Num() - 1)]);
			}
			else
			{
				StepEditor.Pin()->GetEditingAsset<UStepAsset>()->GetRootSteps()[Iter->GetStepIndex()]->OnUnGenerate();
				StepEditor.Pin()->GetEditingAsset<UStepAsset>()->GetRootSteps().RemoveAt(Iter->GetStepIndex());
				StepListItems.RemoveAt(Iter->GetStepIndex());
				//TreeView->SetSelection(StepListItems[FMath::Min(SelectedStepListItem->GetStepIndex(),StepListItems.Num() - 1)]);

				StepEditor.Pin()->GetEditingAsset<UStepAsset>()->Modify();
			}
		}
		UpdateTreeView();
	}

	return FReply::Handled();
}

FReply SStepListWidget::OnClearAllStepItemButtonClicked()
{
	if(!StepEditor.Pin()->GetEditingAsset<UStepAsset>()) return FReply::Handled();

	if(FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("Are you sure to clear all steps?"))) != EAppReturnType::Yes) return FReply::Handled();

	StepEditor.Pin()->GetEditingAsset<UStepAsset>()->ClearAllStep();

	UpdateTreeView(true);

	return FReply::Handled();
}

FReply SStepListWidget::OnMoveUpStepItemButtonClicked()
{
	if(!StepEditor.Pin()->GetEditingAsset<UStepAsset>() || SelectedStepListItems.Num() == 0 || SelectedStepListItems[0]->GetStepIndex() == 0) return FReply::Handled();

	if(SelectedStepListItems[0]->GetParentStep())
	{
		const auto TmpStep = SelectedStepListItems[0]->GetParentSubSteps()[SelectedStepListItems[0]->GetStepIndex()];
		SelectedStepListItems[0]->GetParentSubSteps().RemoveAt(SelectedStepListItems[0]->GetStepIndex());
		SelectedStepListItems[0]->GetParentSubSteps().Insert(TmpStep, SelectedStepListItems[0]->GetStepIndex() - 1);

		SelectedStepListItems[0]->GetParentSubListItems().RemoveAt(SelectedStepListItems[0]->GetStepIndex());
		SelectedStepListItems[0]->GetParentSubListItems().Insert(SelectedStepListItems, SelectedStepListItems[0]->GetStepIndex() - 1);

		SelectedStepListItems[0]->GetParentStep()->Modify();

		UpdateTreeView();
	}
	else
	{
		const auto TmpStep = StepEditor.Pin()->GetEditingAsset<UStepAsset>()->GetRootSteps()[SelectedStepListItems[0]->GetStepIndex()];
		StepEditor.Pin()->GetEditingAsset<UStepAsset>()->GetRootSteps().RemoveAt(SelectedStepListItems[0]->GetStepIndex());
		StepEditor.Pin()->GetEditingAsset<UStepAsset>()->GetRootSteps().Insert(TmpStep, SelectedStepListItems[0]->GetStepIndex() - 1);

		StepListItems.RemoveAt(SelectedStepListItems[0]->GetStepIndex());
		StepListItems.Insert(SelectedStepListItems, SelectedStepListItems[0]->GetStepIndex() - 1);

		StepEditor.Pin()->GetEditingAsset<UStepAsset>()->Modify();

		UpdateTreeView();
	}

	return FReply::Handled();
}

FReply SStepListWidget::OnMoveDownStepItemButtonClicked()
{
	if(!StepEditor.Pin()->GetEditingAsset<UStepAsset>() || SelectedStepListItems.Num() == 0) return FReply::Handled();

	if(SelectedStepListItems[0]->GetParentStep())
	{
		if(SelectedStepListItems[0]->GetStepIndex() < SelectedStepListItems[0]->GetParentSubSteps().Num() - 1)
		{
			const auto TmpStep = SelectedStepListItems[0]->GetParentSubSteps()[SelectedStepListItems[0]->GetStepIndex()];
			SelectedStepListItems[0]->GetParentSubSteps().RemoveAt(SelectedStepListItems[0]->GetStepIndex());
			SelectedStepListItems[0]->GetParentSubSteps().Insert(TmpStep, SelectedStepListItems[0]->GetStepIndex() + 1);

			SelectedStepListItems[0]->GetParentSubListItems().RemoveAt(SelectedStepListItems[0]->GetStepIndex());
			SelectedStepListItems[0]->GetParentSubListItems().Insert(SelectedStepListItems, SelectedStepListItems[0]->GetStepIndex() + 1);

			SelectedStepListItems[0]->GetParentStep()->Modify();

			UpdateTreeView();
		}
	}
	else
	{
		if(SelectedStepListItems[0]->GetStepIndex() < StepEditor.Pin()->GetEditingAsset<UStepAsset>()->GetRootSteps().Num() - 1)
		{
			const auto TmpStep = StepEditor.Pin()->GetEditingAsset<UStepAsset>()->GetRootSteps()[SelectedStepListItems[0]->GetStepIndex()];
			StepEditor.Pin()->GetEditingAsset<UStepAsset>()->GetRootSteps().RemoveAt(SelectedStepListItems[0]->GetStepIndex());
			StepEditor.Pin()->GetEditingAsset<UStepAsset>()->GetRootSteps().Insert(TmpStep, SelectedStepListItems[0]->GetStepIndex() + 1);

			StepListItems.RemoveAt(SelectedStepListItems[0]->GetStepIndex());
			StepListItems.Insert(SelectedStepListItems, SelectedStepListItems[0]->GetStepIndex() + 1);

			StepEditor.Pin()->GetEditingAsset<UStepAsset>()->Modify();

			UpdateTreeView();
		}
	}

	return FReply::Handled();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
