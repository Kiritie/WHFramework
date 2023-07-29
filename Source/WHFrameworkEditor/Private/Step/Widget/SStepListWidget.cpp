// Fill out your copyright notice in the Description page of Project Settings.


#include "Step/Widget/SStepListWidget.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "SlateOptMacros.h"
#include "SourceCodeNavigation.h"
#include "SSkeletonWidget.h"
#include "WHFrameworkEditorStyle.h"
#include "Global/Blueprint/Widget/SCreateBlueprintAssetDialog.h"
#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"
#include "Step/StepBlueprintFactory.h"
#include "Step/StepEditorTypes.h"
#include "Step/StepModule.h"
#include "Step/Base/StepBlueprint.h"
#include "Step/Base/RootStepBase.h"
#include "Step/Widget/SStepListItemWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SStepListWidget::SStepListWidget()
{
	WidgetName = FName("StepListWidget");
	WidgetType = EEditorWidgetType::Child;

	bMultiMode = true;
	bEditMode = false;

	StepModule = nullptr;
	CopiedStep = nullptr;
}

void SStepListWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());

	StepModule = InArgs._StepModule;

	if(!StepModule) return;

	if(!TreeView.IsValid())
	{
		SAssignNew(TreeView, STreeView< TSharedPtr<FStepListItem> >)
			.TreeItemsSource(&StepListItems)
			.OnGenerateRow(this, &SStepListWidget::GenerateTreeRow)
			.OnItemScrolledIntoView(this, &SStepListWidget::TreeItemScrolledIntoView)
			.ItemHeight(18)
			.SelectionMode_Lambda([this](){ return bMultiMode ? ESelectionMode::Multi : ESelectionMode::SingleToggle; })
			.OnSelectionChanged(this, &SStepListWidget::TreeSelectionChanged)
			.OnGetChildren(this, &SStepListWidget::GetChildrenForTree)
			.OnExpansionChanged(this, &SStepListWidget::OnTreeItemExpansionChanged)
			.OnSetExpansionRecursive(this, &SStepListWidget::SetTreeItemExpansionRecursive)
			.ClearSelectionOnClick(false)
			.HighlightParentNodesForSelection(true);
	}

	if(!ScrollBar.IsValid())
	{
		SAssignNew(ScrollBar, SScrollBar)
			.Thickness(FVector2D(9.0f, 9.0f));
	}

	SelectedStepClass = nullptr;

	ClassViewerOptions.bShowBackgroundBorder = false;
	ClassViewerOptions.bShowUnloadedBlueprints = true;
	ClassViewerOptions.bShowNoneOption = false;

	ClassViewerOptions.bIsBlueprintBaseOnly = false;
	ClassViewerOptions.bIsPlaceableOnly = false;
	ClassViewerOptions.NameTypeToDisplay = EClassViewerNameTypeToDisplay::ClassName;
	ClassViewerOptions.DisplayMode = EClassViewerDisplayMode::ListView;
	ClassViewerOptions.bAllowViewOptions = true;

	StepClassFilter = MakeShareable(new FStepClassFilter);
	StepClassFilter->IncludeParentClass = URootStepBase::StaticClass();
	#if ENGINE_MAJOR_VERSION == 4
	ClassViewerOptions.ClassFilter = StepClassFilter;
	#else if ENGINE_MAJOR_VERSION == 5
	ClassViewerOptions.ClassFilters.Add(StepClassFilter.ToSharedRef());
	#endif

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
						.IsEnabled_Lambda([this](){ return SelectedStepClass && SelectedStepListItems.Num() == 1; })
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
						.Text(FText::FromString(TEXT("New")))
						.IsEnabled_Lambda([this](){ return !bEditMode && SelectedStepListItems.Num() <= 1 && SelectedStepClass; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SStepListWidget::OnNewStepItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Insert")))
						.IsEnabled_Lambda([this](){ return !bEditMode && SelectedStepListItems.Num() == 1 && SelectedStepClass; })
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
						.IsEnabled_Lambda([this](){ return !bEditMode && SelectedStepListItems.Num() == 1 && SelectedStepClass; })
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
						.IsEnabled_Lambda([this](){ return SelectedStepListItems.Num() == 1; })
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
						.IsEnabled_Lambda([this](){ return CopiedStep != nullptr; })
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
						.IsEnabled_Lambda([this](){ return SelectedStepListItems.Num() == 1; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SStepListWidget::OnDuplicateStepItemButtonClicked)
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
						.IsEnabled_Lambda([this](){ return StepModule->GetRootSteps().Num() > 0; })
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
						.IsEnabled_Lambda([this](){ return StepModule->GetRootSteps().Num() > 0; })
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
						.IsEnabled_Lambda([this](){ return StepModule->GetRootSteps().Num() > 0; })
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
						.IsEnabled_Lambda([this](){ return SelectedStepListItems.Num() > 0; })
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
						.IsEnabled_Lambda([this](){ return SelectedStepListItems.Num() == 1 && SelectedStepListItems[0]->GetStepIndex() > 0; })
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
							return SelectedStepListItems.Num() == 1 &&
								SelectedStepListItems[0]->GetStepIndex() < (SelectedStepListItems[0]->GetParentStep() ? SelectedStepListItems[0]->GetParentSubSteps().Num() : StepModule->GetRootSteps().Num()) - 1;
						})
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SStepListWidget::OnMoveDownStepItemButtonClicked)
					]
				]
			]
		]
	];

	GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.StepEditorSettings"), TEXT("bMultiMode"), bMultiMode, GStepEditorIni);
	GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.StepEditorSettings"), TEXT("bEditMode"), bEditMode, GStepEditorIni);

	UpdateTreeView(true);

	SetIsMultiMode(bMultiMode);
	SetIsEditMode(bEditMode);
}

void SStepListWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();
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
	SetIsEditMode(bEditMode);
}

void SStepListWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

UStepBase* SStepListWidget::GenerateStep(TSubclassOf<UStepBase> InStepClass)
{
	UStepBase* NewStep = NewObject<UStepBase>(StepModule, InStepClass, NAME_None);

	// NewStep->StepName = *CurrentStepClass->GetName().Replace(TEXT("Step_"), TEXT(""));
	// NewStep->StepDisplayName = FText::FromName(NewStep->StepName);

	NewStep->OnGenerate();

	return NewStep;
}

UStepBase* SStepListWidget::DuplicateStep(UStepBase* InStep)
{
	UStepBase* NewStep = DuplicateObject<UStepBase>(InStep, StepModule, NAME_None);

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

	if(!bEditMode)
	{
		SelectedStepClass = InClass;
	}
	else if(SelectedStepListItems.Num() > 0)
	{
		if(FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("Are you sure to change selected steps class?"))) == EAppReturnType::Yes)
		{
			for(int32 i = 0; i < SelectedStepListItems.Num(); i++)
			{
				UStepBase* OldStep = SelectedStepListItems[i]->Step;
				UStepBase* NewStep = GenerateStep(InClass);

				if(NewStep && OldStep)
				{
					OldStep->OnDuplicate(NewStep);
					if(OldStep->ParentStep)
					{
						OldStep->ParentStep->SubSteps[OldStep->StepIndex] = NewStep;
					}
					else if(URootStepBase* NewRootStep = Cast<URootStepBase>(NewStep))
					{
						StepModule->SetRootStepItem(OldStep->StepIndex, NewRootStep);
					}
					OldStep->OnUnGenerate();
					Refresh();
				}
			}

			StepModule->Modify();

			OnSelectStepListItemsDelegate.Execute(SelectedStepListItems);
			SelectedStepClass = InClass;
		}
	}
}

FText SStepListWidget::GetPickedClassName() const
{
	return FText::FromString(SelectedStepListItems.Num() <= 1 ? (SelectedStepClass ? SelectedStepClass->GetName() : TEXT("None")) : TEXT("Multiple Values"));
}

void SStepListWidget::ToggleEditMode()
{
	SetIsEditMode(!bEditMode);
}

void SStepListWidget::SetIsEditMode(bool bInIsEditMode)
{
	bEditMode = bInIsEditMode;
	if(bEditMode)
	{
		if(SelectedStepListItems.Num() > 0)
		{
			SelectedStepClass = SelectedStepListItems[0]->Step->GetClass();
			if(SelectedStepClass->IsChildOf<URootStepBase>())
			{
				StepClassFilter->IncludeParentClass = URootStepBase::StaticClass();
				StepClassFilter->UnIncludeParentClass = nullptr;
			}
			else
			{
				StepClassFilter->IncludeParentClass = UStepBase::StaticClass();
				StepClassFilter->UnIncludeParentClass = URootStepBase::StaticClass();
			}
		}
		else
		{
			StepClassFilter->IncludeParentClass = nullptr;
			StepClassFilter->UnIncludeParentClass = nullptr;
			SelectedStepClass = nullptr;
		}
	}
	else
	{
		if(SelectedStepListItems.Num() > 0)
		{
			StepClassFilter->IncludeParentClass = UStepBase::StaticClass();
			StepClassFilter->UnIncludeParentClass = URootStepBase::StaticClass();
			SelectedStepClass = UStepBase::StaticClass();
		}
		else
		{
			StepClassFilter->IncludeParentClass = URootStepBase::StaticClass();
			StepClassFilter->UnIncludeParentClass = nullptr;
			SelectedStepClass = URootStepBase::StaticClass();
		}
	}
	GConfig->SetBool(TEXT("/Script/WHFrameworkEditor.StepEditorSettings"), TEXT("bEditMode"), bEditMode, GStepEditorIni);
}

void SStepListWidget::ToggleMultiMode()
{
	SetIsMultiMode(!bMultiMode);
}

void SStepListWidget::SetIsMultiMode(bool bIsMultiMode)
{
	bMultiMode = bIsMultiMode;
	if(!bMultiMode)
	{
		TreeView->ClearSelection();
	}
	GConfig->SetBool(TEXT("/Script/WHFrameworkEditor.StepEditorSettings"), TEXT("bMultiMode"), bMultiMode, GStepEditorIni);
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
	if(!StepModule) return;

	if(bRegenerate)
	{
		StepModule->GenerateListItem(StepListItems);
		for(auto Iter : StepListItems)
		{
			SetTreeItemExpansionRecursive(Iter);
		}
	}
	else
	{
		StepModule->UpdateListItem(StepListItems);
	}
	TreeView->ClearSelection();
	TreeView->RequestTreeRefresh();
}

void SStepListWidget::UpdateSelection()
{
	const TArray<TSharedPtr<FStepListItem>> SelectedItems = TreeView->GetSelectedItems();

	if(SelectedItems.Num() > 0)
	{
		SelectedStepListItems = SelectedItems;
		SelectedStepClass = !bEditMode ? UStepBase::StaticClass() : SelectedStepListItems[0]->Step->GetClass();
		if(SelectedStepClass->IsChildOf<URootStepBase>())
		{
			StepClassFilter->IncludeParentClass = URootStepBase::StaticClass();
			StepClassFilter->UnIncludeParentClass = nullptr;
		}
		else
		{
			StepClassFilter->IncludeParentClass = UStepBase::StaticClass();
			StepClassFilter->UnIncludeParentClass = URootStepBase::StaticClass();
		}
	}
	else
	{
		SelectedStepClass = !bEditMode ? URootStepBase::StaticClass() : nullptr;
		StepClassFilter->IncludeParentClass = URootStepBase::StaticClass();
		StepClassFilter->UnIncludeParentClass = nullptr;
		SelectedStepListItems.Empty();
	}

	if(OnSelectStepListItemsDelegate.IsBound())
	{
		OnSelectStepListItemsDelegate.Execute(SelectedStepListItems);
	}
}

TSharedRef<ITableRow> SStepListWidget::GenerateTreeRow(TSharedPtr<FStepListItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(TreeItem.IsValid());

	return SNew(STableRow<TSharedPtr<FStepListItem>>, OwnerTable)
	[
		SNew(SStepListItemWidget, TreeItem)
	];
}

void SStepListWidget::TreeItemScrolledIntoView(TSharedPtr<FStepListItem> TreeItem, const TSharedPtr<ITableRow>& Widget) { }

void SStepListWidget::GetChildrenForTree(TSharedPtr<FStepListItem> TreeItem, TArray<TSharedPtr<FStepListItem>>& OutChildren)
{
	OutChildren = TreeItem->SubListItems;
}

void SStepListWidget::OnTreeItemExpansionChanged(TSharedPtr<FStepListItem> TreeItem, bool bInExpansionState)
{
	if(TreeItem->GetStates().bExpanded != bInExpansionState)
	{
		TreeItem->GetStates().bExpanded = bInExpansionState;

		TreeItem->Step->Modify();
	}
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
	UpdateSelection();
}

FReply SStepListWidget::OnEditStepItemButtonClicked()
{
	if(!StepModule) return FReply::Handled();

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
	IAssetTools* AssetTools = &FModuleManager::GetModuleChecked<FAssetToolsModule>(FName("AssetTools")).Get();

	FString DefaultAssetPath = TEXT("/Game");
	
	GConfig->GetString(TEXT("/Script/WHFrameworkEditor.StepEditorSettings"), TEXT("DefaultAssetPath"), DefaultAssetPath, GStepEditorIni);

	UStepBlueprintFactory* StepBlueprintFactory = NewObject<UStepBlueprintFactory>(GetTransientPackage(), UStepBlueprintFactory::StaticClass());

	const TSharedRef<SCreateBlueprintAssetDialog> CreateBlueprintAssetDialog = SNew(SCreateBlueprintAssetDialog)
	.DefaultAssetPath(FText::FromString(DefaultAssetPath))
	.BlueprintFactory(StepBlueprintFactory);

	if(CreateBlueprintAssetDialog->ShowModal() != EAppReturnType::Cancel)
	{
		DefaultAssetPath = CreateBlueprintAssetDialog->GetAssetPath();

		const FString AssetName = CreateBlueprintAssetDialog->GetAssetName();
	
		const FString PackageName = CreateBlueprintAssetDialog->GetPackageName();

		GConfig->SetString(TEXT("/Script/WHFrameworkEditor.StepEditorSettings"), TEXT("DefaultAssetPath"), *DefaultAssetPath, GStepEditorIni);
	
		if(UStepBlueprint* StepBlueprintAsset = Cast<UStepBlueprint>(AssetTools->CreateAsset(AssetName, DefaultAssetPath, UStepBlueprint::StaticClass(), StepBlueprintFactory, FName("ContentBrowserNewAsset"))))
		{
			TArray<UObject*> ObjectsToSyncTo;
			ObjectsToSyncTo.Add(StepBlueprintAsset);

			GEditor->SyncBrowserToObjects(ObjectsToSyncTo);

			//SelectedStepClass = StepBlueprintAsset->GetClass();
		}
	}

	return FReply::Handled();
}

FReply SStepListWidget::OnNewStepItemButtonClicked()
{
	if(!StepModule) return FReply::Handled();

	UStepBase* NewStep = GenerateStep(SelectedStepClass);

	const auto Item = MakeShared<FStepListItem>();
	Item->Step = NewStep;

	StepModule->GetStepMap().Add(NewStep->StepGUID, NewStep);

	if(SelectedStepListItems.Num() > 0)
	{
		if(SelectedStepListItems[0]->Step->GetStepType() == EStepType::Standalone)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Cannot be added under a standalone Step!")));
			return FReply::Handled();
		}
		Item->ParentListItem = SelectedStepListItems[0];

		NewStep->StepIndex = SelectedStepListItems[0]->GetSubSteps().Num();
		NewStep->StepHierarchy = SelectedStepListItems[0]->Step->StepHierarchy + 1;
		SelectedStepListItems[0]->GetSubSteps().Add(NewStep);
		SelectedStepListItems[0]->SubListItems.Add(Item);

		SelectedStepListItems[0]->Step->Modify();
	}
	else if(URootStepBase* RootStep = Cast<URootStepBase>(NewStep))
	{
		NewStep->StepIndex = StepModule->GetRootSteps().Num();
		NewStep->StepHierarchy = 0;
		StepModule->GetRootSteps().Add(RootStep);
		StepListItems.Add(Item);

		StepModule->Modify();
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
	if(!StepModule) return FReply::Handled();

	if(SelectedStepListItems.Num() > 0)
	{
		UStepBase* NewStep = GenerateStep(SelectedStepClass);

		const auto Item = MakeShared<FStepListItem>();
		Item->Step = NewStep;
		Item->ParentListItem = SelectedStepListItems[0]->ParentListItem;

		StepModule->GetStepMap().Add(NewStep->StepGUID, NewStep);

		if(SelectedStepListItems[0]->GetParentStep())
		{
			SelectedStepListItems[0]->GetParentSubSteps().Insert(NewStep, SelectedStepListItems[0]->Step->StepIndex);
			SelectedStepListItems[0]->GetParentSubListItems().Insert(Item, SelectedStepListItems[0]->Step->StepIndex);

			SelectedStepListItems[0]->GetParentStep()->Modify();
		}
		else if(URootStepBase* NewRootStep = Cast<URootStepBase>(NewStep))
		{
			StepModule->GetRootSteps().Insert(NewRootStep, SelectedStepListItems[0]->Step->StepIndex);
			StepListItems.Insert(Item, SelectedStepListItems[0]->Step->StepIndex);

			StepModule->Modify();
		}

		TreeView->SetSelection(Item);
		UpdateTreeView();
	}

	return FReply::Handled();
}

FReply SStepListWidget::OnAppendStepItemButtonClicked()
{
	if(!StepModule) return FReply::Handled();

	if(SelectedStepListItems.Num() > 0)
	{
		UStepBase* NewStep = GenerateStep(SelectedStepClass);

		const auto Item = MakeShared<FStepListItem>();
		Item->Step = NewStep;
		Item->ParentListItem = SelectedStepListItems[0]->ParentListItem;

		StepModule->GetStepMap().Add(NewStep->StepGUID, NewStep);

		if(SelectedStepListItems[0]->GetParentStep())
		{
			SelectedStepListItems[0]->GetParentSubSteps().Insert(NewStep, SelectedStepListItems[0]->Step->StepIndex + 1);
			SelectedStepListItems[0]->GetParentSubListItems().Insert(Item, SelectedStepListItems[0]->Step->StepIndex + 1);

			SelectedStepListItems[0]->GetParentStep()->Modify();
		}
		else if(URootStepBase* NewRootStep = Cast<URootStepBase>(NewStep))
		{
			StepModule->GetRootSteps().Insert(NewRootStep, SelectedStepListItems[0]->Step->StepIndex + 1);
			StepListItems.Insert(Item, SelectedStepListItems[0]->Step->StepIndex + 1);

			StepModule->Modify();
		}

		TreeView->SetSelection(Item);
		UpdateTreeView();
	}

	return FReply::Handled();
}

FReply SStepListWidget::OnCopyStepItemButtonClicked()
{
	if(!StepModule) return FReply::Handled();

	CopiedStep = DuplicateStep(SelectedStepListItems[0]->Step);

	return FReply::Handled();
}

FReply SStepListWidget::OnPasteStepItemButtonClicked()
{
	if(!StepModule || !CopiedStep) return FReply::Handled();

	const auto Item = MakeShared<FStepListItem>();
	Item->Step = CopiedStep;

	StepModule->GetStepMap().Add(CopiedStep->StepGUID, CopiedStep);

	if(SelectedStepListItems.Num() > 0)
	{
		if(SelectedStepListItems[0]->Step->GetStepType() == EStepType::Standalone)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Cannot be pasted under a standalone Step!")));
			return FReply::Handled();
		}
		Item->ParentListItem = SelectedStepListItems[0];

		CopiedStep->StepIndex = SelectedStepListItems[0]->GetSubSteps().Num();
		CopiedStep->StepHierarchy = SelectedStepListItems[0]->Step->StepHierarchy;
		SelectedStepListItems[0]->GetSubSteps().Add(CopiedStep);
		SelectedStepListItems[0]->SubListItems.Add(Item);

		SelectedStepListItems[0]->Step->Modify();
	}
	else if(URootStepBase* RootStep = Cast<URootStepBase>(CopiedStep))
	{
		CopiedStep->StepIndex = StepModule->GetRootSteps().Num();
		CopiedStep->StepHierarchy = 0;
		StepModule->GetRootSteps().Add(RootStep);
		StepListItems.Add(Item);

		StepModule->Modify();
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
	if(!StepModule) return FReply::Handled();

	if(SelectedStepListItems.Num() > 0)
	{
		UStepBase* NewStep = DuplicateStep(SelectedStepListItems[0]->Step);

		const auto Item = MakeShared<FStepListItem>();
		Item->Step = NewStep;
		Item->ParentListItem = SelectedStepListItems[0]->ParentListItem;

		StepModule->GetStepMap().Add(NewStep->StepGUID, NewStep);

		if(SelectedStepListItems[0]->GetParentStep())
		{
			SelectedStepListItems[0]->GetParentSubSteps().Insert(NewStep, SelectedStepListItems[0]->Step->StepIndex + 1);
			SelectedStepListItems[0]->GetParentSubListItems().Insert(Item, SelectedStepListItems[0]->Step->StepIndex + 1);

			SelectedStepListItems[0]->GetParentStep()->Modify();
		}
		else if(URootStepBase* NewRootStep = Cast<URootStepBase>(NewStep))
		{
			StepModule->GetRootSteps().Insert(NewRootStep, SelectedStepListItems[0]->Step->StepIndex + 1);
			StepListItems.Insert(Item, SelectedStepListItems[0]->Step->StepIndex + 1);

			StepModule->Modify();
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
	if(!StepModule) return FReply::Handled();

	if(FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("Are you sure to remove selected steps?"))) != EAppReturnType::Yes) return FReply::Handled();

	if(SelectedStepListItems.Num() > 0)
	{
		for(auto Iter : SelectedStepListItems)
		{
			StepModule->GetStepMap().Remove(Iter->Step->StepGUID);

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
				StepModule->GetRootSteps()[Iter->GetStepIndex()]->OnUnGenerate();
				StepModule->GetRootSteps().RemoveAt(Iter->GetStepIndex());
				StepListItems.RemoveAt(Iter->GetStepIndex());
				//TreeView->SetSelection(StepListItems[FMath::Min(SelectedStepListItem->GetStepIndex(),StepListItems.Num() - 1)]);

				StepModule->Modify();
			}
		}
		UpdateTreeView();
	}

	return FReply::Handled();
}

FReply SStepListWidget::OnClearAllStepItemButtonClicked()
{
	if(!StepModule) return FReply::Handled();

	if(FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("Are you sure to clear all steps?"))) != EAppReturnType::Yes) return FReply::Handled();

	StepModule->ClearAllStep();

	UpdateTreeView(true);

	return FReply::Handled();
}

FReply SStepListWidget::OnMoveUpStepItemButtonClicked()
{
	if(!StepModule || SelectedStepListItems.Num() == 0 || SelectedStepListItems[0]->GetStepIndex() == 0) return FReply::Handled();

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
		const auto TmpStep = StepModule->GetRootSteps()[SelectedStepListItems[0]->GetStepIndex()];
		StepModule->GetRootSteps().RemoveAt(SelectedStepListItems[0]->GetStepIndex());
		StepModule->GetRootSteps().Insert(TmpStep, SelectedStepListItems[0]->GetStepIndex() - 1);

		StepListItems.RemoveAt(SelectedStepListItems[0]->GetStepIndex());
		StepListItems.Insert(SelectedStepListItems, SelectedStepListItems[0]->GetStepIndex() - 1);

		StepModule->Modify();

		UpdateTreeView();
	}

	return FReply::Handled();
}

FReply SStepListWidget::OnMoveDownStepItemButtonClicked()
{
	if(!StepModule || SelectedStepListItems.Num() == 0) return FReply::Handled();

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
		if(SelectedStepListItems[0]->GetStepIndex() < StepModule->GetRootSteps().Num() - 1)
		{
			const auto TmpStep = StepModule->GetRootSteps()[SelectedStepListItems[0]->GetStepIndex()];
			StepModule->GetRootSteps().RemoveAt(SelectedStepListItems[0]->GetStepIndex());
			StepModule->GetRootSteps().Insert(TmpStep, SelectedStepListItems[0]->GetStepIndex() + 1);

			StepListItems.RemoveAt(SelectedStepListItems[0]->GetStepIndex());
			StepListItems.Insert(SelectedStepListItems, SelectedStepListItems[0]->GetStepIndex() + 1);

			StepModule->Modify();

			UpdateTreeView();
		}
	}

	return FReply::Handled();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
