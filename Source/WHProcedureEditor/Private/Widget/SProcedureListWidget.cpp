// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/SProcedureListWidget.h"
#include "SlateOptMacros.h"
#include "SourceCodeNavigation.h"
#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"
#include "Procedure/ProcedureModule.h"
#include "Procedure/Base/RootProcedureBase.h"
#include "Slate/Private/Widgets/Views/SListPanel.h"
#include "Widget/SProcedureListItemWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SProcedureListWidget::Construct(const FArguments& InArgs)
{
	MainModule = InArgs._MainModule;
	ProcedureModule = InArgs._ProcedureModule;
	
	if(!MainModule || !ProcedureModule) return;

	if (!TreeView.IsValid())
	{
		SAssignNew(TreeView, STreeView< TSharedPtr<FProcedureListItem> >)
			.TreeItemsSource(&ProcedureListItems)
			.OnGenerateRow(this, &SProcedureListWidget::GenerateTreeRow)
			.OnItemScrolledIntoView(this, &SProcedureListWidget::TreeItemScrolledIntoView)
			.ItemHeight(18)
			.SelectionMode_Lambda([this](){ return bCurrentIsMultiMode ? ESelectionMode::Multi : ESelectionMode::SingleToggle; })
			.OnSelectionChanged(this, &SProcedureListWidget::TreeSelectionChanged)
			.OnGetChildren(this, &SProcedureListWidget::GetChildrenForTree)
			.OnSetExpansionRecursive(this, &SProcedureListWidget::SetTreeItemExpansionRecursive)
			.ClearSelectionOnClick(false)
			.HighlightParentNodesForSelection(true);
	}

	if(!ScrollBar.IsValid())
	{
		SAssignNew(ScrollBar, SScrollBar)
		.Thickness(FVector2D(9.0f, 9.0f));
	}

	CurrentProcedureClass = ARootProcedureBase::StaticClass();

	GConfig->GetBool(TEXT("/Script/WHProcedureEditor.WHProcedureEditorSettings"), TEXT("bDefaultIsMultiMode"), bCurrentIsMultiMode, GProcedureEditorIni);
	GConfig->GetBool(TEXT("/Script/WHProcedureEditor.WHProcedureEditorSettings"), TEXT("bDefaultIsEditMode"), bCurrentIsEditMode, GProcedureEditorIni);

	ClassViewerOptions.bShowBackgroundBorder = false;
	ClassViewerOptions.bShowUnloadedBlueprints = true;
	ClassViewerOptions.bShowNoneOption = false;

	ClassViewerOptions.bIsBlueprintBaseOnly = false;
	ClassViewerOptions.bIsPlaceableOnly = false;
	ClassViewerOptions.NameTypeToDisplay = EClassViewerNameTypeToDisplay::ClassName;
	ClassViewerOptions.DisplayMode = EClassViewerDisplayMode::ListView;
	ClassViewerOptions.bAllowViewOptions = true;

	ProcedureClassFilter = MakeShareable(new FProcedureClassFilter);
	ProcedureClassFilter->IncludeParentClass = ARootProcedureBase::StaticClass();
	ClassViewerOptions.ClassFilter = ProcedureClassFilter;

	SAssignNew(ClassPickButton, SComboButton)
	.OnGetMenuContent(this, &SProcedureListWidget::GenerateClassPicker)
	.ContentPadding(FMargin(2.0f, 2.0f))
	.ToolTipText(this, &SProcedureListWidget::GetPickedClassName)
	.ButtonContent()
	[
		SNew(STextBlock)
		.Text(this, &SProcedureListWidget::GetPickedClassName)
	];

	ChildSlot
	[
		SNew(SBorder)
		.Padding(1.f)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SBox)
			.MaxDesiredWidth(360)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.AutoHeight()
				[
					SNew(SHorizontalBox)
							
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)	
					.HAlign(HAlign_Center)	
					.Padding(5.f, 2.f, 5.f, 2.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Procedure List")))
					]
				]

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
						.Text(FText::FromString(TEXT("Class: ")))
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
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					.AutoWidth()
					.Padding(2.f, 0, 0, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Edit")))
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SProcedureListWidget::OnEditProcedureItemButtonClicked)
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
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("New")))
						.IsEnabled_Lambda([this](){ return !bCurrentIsEditMode && SelectedProcedureListItems.Num() <= 1; ; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SProcedureListWidget::OnNewProcedureItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Insert")))
						.IsEnabled_Lambda([this](){ return !bCurrentIsEditMode && SelectedProcedureListItems.Num() == 1; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SProcedureListWidget::OnInsertProcedureItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Append")))
						.IsEnabled_Lambda([this](){ return !bCurrentIsEditMode && SelectedProcedureListItems.Num() == 1; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SProcedureListWidget::OnAppendProcedureItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Copy")))
						.IsEnabled_Lambda([this](){ return SelectedProcedureListItems.Num() == 1; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SProcedureListWidget::OnCopyProcedureItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Paste")))
						.IsEnabled_Lambda([this](){ return CopiedProcedure != nullptr; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SProcedureListWidget::OnPasteProcedureItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					.AutoWidth()
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Duplicate")))
						.IsEnabled_Lambda([this](){ return SelectedProcedureListItems.Num() == 1; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SProcedureListWidget::OnDuplicateProcedureItemButtonClicked)
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
					.AutoWidth()
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("Expand")))
						.IsEnabled_Lambda([this](){ return ProcedureModule->RootProcedures.Num() > 0; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SProcedureListWidget::OnExpandAllProcedureItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					.AutoWidth()
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("Collapse")))
						.IsEnabled_Lambda([this](){ return ProcedureModule->RootProcedures.Num() > 0; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SProcedureListWidget::OnCollapseAllProcedureItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					.AutoWidth()
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("Clear All")))
						.IsEnabled_Lambda([this](){ return ProcedureModule->RootProcedures.Num() > 0; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SProcedureListWidget::OnClearAllProcedureItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					.AutoWidth()
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("Remove")))
						.IsEnabled_Lambda([this](){ return SelectedProcedureListItems.Num() > 0; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SProcedureListWidget::OnRemoveProcedureItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					.AutoWidth()
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("Move Up")))
						.IsEnabled_Lambda([this](){ return SelectedProcedureListItems.Num() == 1 && SelectedProcedureListItems[0]->GetProcedureIndex() > 0; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SProcedureListWidget::OnMoveUpProcedureItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					.AutoWidth()
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("Move Down")))
						.IsEnabled_Lambda([this](){ return SelectedProcedureListItems.Num() == 1 &&
							SelectedProcedureListItems[0]->GetProcedureIndex() < (SelectedProcedureListItems[0]->GetParentProcedure() ? SelectedProcedureListItems[0]->GetParentSubProcedures().Num() : ProcedureModule->RootProcedures.Num()) - 1; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SProcedureListWidget::OnMoveDownProcedureItemButtonClicked)
					]
				]
			]
		]
	];

	UpdateTreeView(true);
}

AProcedureBase* SProcedureListWidget::GenerateProcedure(TSubclassOf<AProcedureBase> InProcedureClass)
{
	AProcedureBase* NewProcedure = NewObject<AProcedureBase>(ProcedureModule, InProcedureClass, NAME_None, RF_ArchetypeObject | RF_Transactional | RF_Public);
	
	// NewProcedure->ProcedureName = *CurrentProcedureClass->GetName().Replace(TEXT("Procedure_"), TEXT(""));
	// NewProcedure->ProcedureDisplayName = FText::FromName(NewProcedure->ProcedureName);

	NewProcedure->OnGenerate();

	return NewProcedure;
}

AProcedureBase* SProcedureListWidget::DuplicateProcedure(AProcedureBase* InProcedure)
{
	AProcedureBase* NewProcedure = DuplicateObject<AProcedureBase>(InProcedure, ProcedureModule, NAME_None);

	NewProcedure->SubProcedures.Empty();

	// NewProcedure->ProcedureName = *FString::Printf(TEXT("%s_Copy"), *NewProcedure->ProcedureName.ToString());
	// NewProcedure->ProcedureDisplayName = FText::FromString(FString::Printf(TEXT("%s_Copy"), *NewProcedure->ProcedureDisplayName.ToString()));

	NewProcedure->OnGenerate();

	return NewProcedure;
}

TSharedRef<SWidget> SProcedureListWidget::GenerateClassPicker()
{
	FOnClassPicked OnPicked(FOnClassPicked::CreateRaw(this, &SProcedureListWidget::OnClassPicked));

	return SNew(SBox)
		.WidthOverride(280)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(500)
			[
				FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(ClassViewerOptions, OnPicked)
			]		
		];
}

void SProcedureListWidget::OnClassPicked(UClass* InClass)
{
	ClassPickButton->SetIsOpen(false);

	if(!bCurrentIsEditMode)
	{
		CurrentProcedureClass = InClass;
	}
	else if(SelectedProcedureListItems.Num() > 0)
	{
		if(FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("Are you sure to change selected procedures class?"))) == EAppReturnType::Yes)
		{
			for(int32 i = 0; i < SelectedProcedureListItems.Num(); i++)
			{
				AProcedureBase* OldProcedure = SelectedProcedureListItems[i]->Procedure;
				AProcedureBase* NewProcedure = GenerateProcedure(InClass);
				
				if(NewProcedure && OldProcedure && OldProcedure->ParentProcedure)
				{
					NewProcedure->ProcedureName = OldProcedure->ProcedureName;
					NewProcedure->ProcedureDisplayName = OldProcedure->ProcedureDisplayName;
					NewProcedure->ProcedureDescription = OldProcedure->ProcedureDescription;
					NewProcedure->ProcedureIndex = OldProcedure->ProcedureIndex;
					NewProcedure->bAutoEnterProcedure = OldProcedure->bAutoEnterProcedure;
					NewProcedure->bAutoLeaveProcedure = OldProcedure->bAutoLeaveProcedure;
					NewProcedure->ProcedureExecuteType = OldProcedure->ProcedureExecuteType;
					NewProcedure->ProcedureExecuteCondition = OldProcedure->ProcedureExecuteCondition;
					NewProcedure->ParentProcedure = OldProcedure->ParentProcedure;
					NewProcedure->bMergeSubProcedure = OldProcedure->bMergeSubProcedure;
					NewProcedure->SubProcedures = OldProcedure->SubProcedures;

					OldProcedure->ParentProcedure->SubProcedures[OldProcedure->ProcedureIndex] = NewProcedure;
					SelectedProcedureListItems[i]->Procedure = NewProcedure;
					OldProcedure->OnUnGenerate();
				}
			}
			
			OnSelectProcedureListItemsDelegate.Execute(SelectedProcedureListItems);
			CurrentProcedureClass = InClass;
		}
	}
}

FText SProcedureListWidget::GetPickedClassName() const
{
	return FText::FromString(CurrentProcedureClass ? CurrentProcedureClass->GetName() : TEXT("None"));
}

void SProcedureListWidget::ToggleEditMode()
{
	bCurrentIsEditMode = !bCurrentIsEditMode;
	if(bCurrentIsEditMode)
	{
		if(SelectedProcedureListItems.Num() > 0)
		{
			CurrentProcedureClass = SelectedProcedureListItems[0]->Procedure->GetClass();
		}
	}
	GConfig->SetBool(TEXT("/Script/WHProcedureEditor.WHProcedureEditorSettings"), TEXT("bDefaultIsEditMode"), bCurrentIsEditMode, GProcedureEditorIni);
}

void SProcedureListWidget::ToggleMultiMode()
{
	bCurrentIsMultiMode = !bCurrentIsMultiMode;
	if(!bCurrentIsMultiMode)
	{
		TreeView->ClearSelection();
	}
	GConfig->SetBool(TEXT("/Script/WHProcedureEditor.WHProcedureEditorSettings"), TEXT("bDefaultIsMultiMode"), bCurrentIsMultiMode, GProcedureEditorIni);
}

int32 SProcedureListWidget::GetTotalProcedureNum() const
{
	int32 TotalNum = 0;
	for(auto Iter : ProcedureListItems)
	{
		TotalNum++;
		Iter->GetSubProcedureNum(TotalNum);
	}
	return TotalNum;
}

int32 SProcedureListWidget::GetSelectedProcedureNum() const
{
	return SelectedProcedureListItems.Num();
}

void SProcedureListWidget::UpdateTreeView(bool bRegenerate)
{
	if(!MainModule || !ProcedureModule) return;

	if(bRegenerate)
	{
		ProcedureModule->GenerateListItem(ProcedureListItems);
		for (auto Iter : ProcedureListItems)
		{
			SetTreeItemExpansionRecursive(Iter, true);
		}
	}
	else
	{
		ProcedureModule->UpdateListItem(ProcedureListItems);
	}
	
	TreeView->RequestTreeRefresh();
}

TSharedRef<ITableRow> SProcedureListWidget::GenerateTreeRow( TSharedPtr<FProcedureListItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	check(TreeItem.IsValid());

	return SNew(STableRow<TSharedPtr<FProcedureListItem>>, OwnerTable)
	[
		SNew(SProcedureListItemWidget, TreeItem)
	];
}

void SProcedureListWidget::TreeItemScrolledIntoView( TSharedPtr<FProcedureListItem> TreeItem, const TSharedPtr<ITableRow>& Widget )
{
	
}

void SProcedureListWidget::GetChildrenForTree( TSharedPtr< FProcedureListItem > TreeItem, TArray< TSharedPtr<FProcedureListItem> >& OutChildren )
{
	OutChildren = TreeItem->SubListItems;
}

void SProcedureListWidget::SetTreeItemExpansionRecursive( TSharedPtr< FProcedureListItem > TreeItem, bool bInExpansionState )
{
	TreeView->SetItemExpansion(TreeItem, bInExpansionState);
	
	for(auto Iter : TreeItem->SubListItems)
	{
		SetTreeItemExpansionRecursive( Iter, bInExpansionState );
	}
}

void SProcedureListWidget::TreeSelectionChanged( TSharedPtr< FProcedureListItem > TreeItem, ESelectInfo::Type SelectInfo )
{
	const TArray<TSharedPtr<FProcedureListItem>> SelectedItems = TreeView->GetSelectedItems();

	if(SelectedItems.Num() > 0)
	{
		if(SelectedProcedureListItems.Num() == 0)
		{
			CurrentProcedureClass = AProcedureBase::StaticClass();
			ProcedureClassFilter->UnIncludeParentClass = ARootProcedureBase::StaticClass();
			ProcedureClassFilter->IncludeParentClass = CurrentProcedureClass;
		}
		SelectedProcedureListItems = SelectedItems;
		if(bCurrentIsEditMode)
		{
			CurrentProcedureClass = SelectedProcedureListItems[0]->Procedure->GetClass();
		}
	}
	else
	{
		if(SelectedProcedureListItems.Num() > 0)
		{
			CurrentProcedureClass = ARootProcedureBase::StaticClass();
			ProcedureClassFilter->UnIncludeParentClass = nullptr;
			ProcedureClassFilter->IncludeParentClass = CurrentProcedureClass;
		}
		SelectedProcedureListItems.Empty();
	}

	if(OnSelectProcedureListItemsDelegate.IsBound())
	{
		OnSelectProcedureListItemsDelegate.Execute(SelectedProcedureListItems);
	}
}

FReply SProcedureListWidget::OnEditProcedureItemButtonClicked()
{
	if(!MainModule || !ProcedureModule) return FReply::Unhandled();

	if (CurrentProcedureClass->ClassGeneratedBy)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset( CurrentProcedureClass->ClassGeneratedBy );
	}
	else
	{
		FSourceCodeNavigation::NavigateToClass(CurrentProcedureClass);

	}
	return FReply::Handled();
}

FReply SProcedureListWidget::OnNewProcedureItemButtonClicked()
{
	if(!MainModule || !ProcedureModule) return FReply::Unhandled();

	AProcedureBase* NewProcedure = GenerateProcedure(CurrentProcedureClass);
	
	const auto Item = MakeShared<FProcedureListItem>();
	Item->Procedure = NewProcedure;

	if(SelectedProcedureListItems.Num() > 0)
	{
		if(SelectedProcedureListItems[0]->Procedure->GetProcedureType() == EProcedureType::Standalone)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Cannot be added under a standalone procedure!")));
			return FReply::Handled();
		}
		Item->ParentListItem = SelectedProcedureListItems[0];

		NewProcedure->ProcedureIndex = SelectedProcedureListItems[0]->GetSubProcedures().Num();
		SelectedProcedureListItems[0]->GetSubProcedures().Add(NewProcedure);
		SelectedProcedureListItems[0]->SubListItems.Add(Item);

		SelectedProcedureListItems[0]->Procedure->Modify();
	}
	else if(ARootProcedureBase* RootProcedure = Cast<ARootProcedureBase>(NewProcedure))
	{
		NewProcedure->ProcedureIndex = ProcedureModule->RootProcedures.Num();
		ProcedureModule->RootProcedures.Add(RootProcedure);
		ProcedureListItems.Add(Item);

		ProcedureModule->Modify();
	}

	if(Item->ParentListItem)
	{
		TreeView->SetItemExpansion(Item->ParentListItem, true);
	}
	TreeView->SetSelection(Item);
	UpdateTreeView();

	return FReply::Handled();
}

FReply SProcedureListWidget::OnInsertProcedureItemButtonClicked()
{
	if(!MainModule || !ProcedureModule) return FReply::Unhandled();

	if(SelectedProcedureListItems.Num() > 0)
	{
		AProcedureBase* NewProcedure = GenerateProcedure(CurrentProcedureClass);
	
		const auto Item = MakeShared<FProcedureListItem>();
		Item->Procedure = NewProcedure;
		Item->ParentListItem = SelectedProcedureListItems[0]->ParentListItem;

		if(SelectedProcedureListItems[0]->GetParentProcedure())
		{
			SelectedProcedureListItems[0]->GetParentSubProcedures().Insert(NewProcedure, SelectedProcedureListItems[0]->Procedure->ProcedureIndex);
			SelectedProcedureListItems[0]->GetParentSubListItems().Insert(Item, SelectedProcedureListItems[0]->Procedure->ProcedureIndex);

			SelectedProcedureListItems[0]->GetParentProcedure()->Modify();
		}
		else if(ARootProcedureBase* NewRootProcedure = Cast<ARootProcedureBase>(NewProcedure))
		{
			ProcedureModule->RootProcedures.Insert(NewRootProcedure, SelectedProcedureListItems[0]->Procedure->ProcedureIndex);
			ProcedureListItems.Insert(Item, SelectedProcedureListItems[0]->Procedure->ProcedureIndex);

			ProcedureModule->Modify();
		}
		
		TreeView->SetSelection(Item);
		UpdateTreeView();
	}

	return FReply::Handled();
}

FReply SProcedureListWidget::OnAppendProcedureItemButtonClicked()
{
	if(!MainModule || !ProcedureModule) return FReply::Unhandled();

	if(SelectedProcedureListItems.Num() > 0)
	{
		AProcedureBase* NewProcedure = GenerateProcedure(CurrentProcedureClass);
	
		const auto Item = MakeShared<FProcedureListItem>();
		Item->Procedure = NewProcedure;
		Item->ParentListItem = SelectedProcedureListItems[0]->ParentListItem;

		if(SelectedProcedureListItems[0]->GetParentProcedure())
		{
			SelectedProcedureListItems[0]->GetParentSubProcedures().Insert(NewProcedure, SelectedProcedureListItems[0]->Procedure->ProcedureIndex + 1);
			SelectedProcedureListItems[0]->GetParentSubListItems().Insert(Item, SelectedProcedureListItems[0]->Procedure->ProcedureIndex + 1);

			SelectedProcedureListItems[0]->GetParentProcedure()->Modify();
		}
		else if(ARootProcedureBase* NewRootProcedure = Cast<ARootProcedureBase>(NewProcedure))
		{
			ProcedureModule->RootProcedures.Insert(NewRootProcedure, SelectedProcedureListItems[0]->Procedure->ProcedureIndex + 1);
			ProcedureListItems.Insert(Item, SelectedProcedureListItems[0]->Procedure->ProcedureIndex + 1);

			ProcedureModule->Modify();
		}
		
		TreeView->SetSelection(Item);
		UpdateTreeView();
	}

	return FReply::Handled();
}

FReply SProcedureListWidget::OnCopyProcedureItemButtonClicked()
{
	if(!MainModule || !ProcedureModule) return FReply::Unhandled();

	CopiedProcedure = DuplicateProcedure(SelectedProcedureListItems[0]->Procedure);

	return FReply::Handled();
}

FReply SProcedureListWidget::OnPasteProcedureItemButtonClicked()
{
	if(!MainModule || !ProcedureModule || !CopiedProcedure) return FReply::Unhandled();

	const auto Item = MakeShared<FProcedureListItem>();
	Item->Procedure = CopiedProcedure;

	if(SelectedProcedureListItems.Num() > 0)
	{
		if(SelectedProcedureListItems[0]->Procedure->GetProcedureType() == EProcedureType::Standalone)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Cannot be pasted under a standalone procedure!")));
			return FReply::Handled();
		}
		Item->ParentListItem = SelectedProcedureListItems[0];

		CopiedProcedure->ProcedureIndex = SelectedProcedureListItems[0]->GetSubProcedures().Num();
		SelectedProcedureListItems[0]->GetSubProcedures().Add(CopiedProcedure);
		SelectedProcedureListItems[0]->SubListItems.Add(Item);

		SelectedProcedureListItems[0]->Procedure->Modify();
	}
	else if(ARootProcedureBase* RootProcedure = Cast<ARootProcedureBase>(CopiedProcedure))
	{
		CopiedProcedure->ProcedureIndex = ProcedureModule->RootProcedures.Num();
		ProcedureModule->RootProcedures.Add(RootProcedure);
		ProcedureListItems.Add(Item);

		ProcedureModule->Modify();
	}
	
	if(Item->ParentListItem)
	{
		TreeView->SetItemExpansion(Item->ParentListItem, true);
	}
	TreeView->SetSelection(Item);
	UpdateTreeView();

	CopiedProcedure = nullptr;

	return FReply::Handled();
}

FReply SProcedureListWidget::OnDuplicateProcedureItemButtonClicked()
{
	if(!MainModule || !ProcedureModule) return FReply::Unhandled();

	if(SelectedProcedureListItems.Num() > 0)
	{
		AProcedureBase* NewProcedure = DuplicateProcedure(SelectedProcedureListItems[0]->Procedure);
	
		const auto Item = MakeShared<FProcedureListItem>();
		Item->Procedure = NewProcedure;
		Item->ParentListItem = SelectedProcedureListItems[0]->ParentListItem;

		if(SelectedProcedureListItems[0]->GetParentProcedure())
		{
			SelectedProcedureListItems[0]->GetParentSubProcedures().Insert(NewProcedure, SelectedProcedureListItems[0]->Procedure->ProcedureIndex + 1);
			SelectedProcedureListItems[0]->GetParentSubListItems().Insert(Item, SelectedProcedureListItems[0]->Procedure->ProcedureIndex + 1);

			SelectedProcedureListItems[0]->GetParentProcedure()->Modify();
		}
		else if(ARootProcedureBase* NewRootProcedure = Cast<ARootProcedureBase>(NewProcedure))
		{
			ProcedureModule->RootProcedures.Insert(NewRootProcedure, SelectedProcedureListItems[0]->Procedure->ProcedureIndex + 1);
			ProcedureListItems.Insert(Item, SelectedProcedureListItems[0]->Procedure->ProcedureIndex + 1);

			ProcedureModule->Modify();
		}
		
		TreeView->SetSelection(Item);
		UpdateTreeView();
	}

	return FReply::Handled();
}

FReply SProcedureListWidget::OnExpandAllProcedureItemButtonClicked()
{
	for (auto Iter : ProcedureListItems)
	{
		SetTreeItemExpansionRecursive(Iter, true);
	}

	return FReply::Handled();
}

FReply SProcedureListWidget::OnCollapseAllProcedureItemButtonClicked()
{
	for (auto Iter : ProcedureListItems)
	{
		SetTreeItemExpansionRecursive(Iter, false);
	}

	return FReply::Handled();
}

FReply SProcedureListWidget::OnRemoveProcedureItemButtonClicked()
{
	if(!MainModule || !ProcedureModule) return FReply::Unhandled();

	if(FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("Are you sure to remove selected procedures?"))) != EAppReturnType::Yes) return FReply::Handled();

	if(SelectedProcedureListItems.Num() > 0)
	{
		for(auto Iter : SelectedProcedureListItems)
		{
			if(Iter->GetParentProcedure())
			{
				Iter->GetParentSubProcedures()[Iter->GetProcedureIndex()]->OnUnGenerate();
				Iter->GetParentSubProcedures().RemoveAt(Iter->GetProcedureIndex());
				Iter->GetParentSubListItems().RemoveAt(Iter->GetProcedureIndex());

				Iter->GetParentProcedure()->Modify();

				//TreeView->SetSelection(SelectedProcedureListItem->GetParentSubListItems()[FMath::Min(SelectedProcedureListItem->GetProcedureIndex(), SelectedProcedureListItem->GetParentSubListItems().Num() - 1)]);
			}
			else
			{
				ProcedureModule->RootProcedures[Iter->GetProcedureIndex()]->OnUnGenerate();
				ProcedureModule->RootProcedures.RemoveAt(Iter->GetProcedureIndex());
				ProcedureListItems.RemoveAt(Iter->GetProcedureIndex());
				//TreeView->SetSelection(ProcedureListItems[FMath::Min(SelectedProcedureListItem->GetProcedureIndex(),ProcedureListItems.Num() - 1)]);

				ProcedureModule->Modify();
			}
			UpdateTreeView();
		}
	}

	return FReply::Handled();
}

FReply SProcedureListWidget::OnClearAllProcedureItemButtonClicked()
{
	if(!MainModule || !ProcedureModule) return FReply::Unhandled();

	if(FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("Are you sure to clear all procedures?"))) != EAppReturnType::Yes) return FReply::Handled();

	ProcedureModule->ClearAllProcedure();
	
	UpdateTreeView(true);

	return FReply::Handled();
}

FReply SProcedureListWidget::OnMoveUpProcedureItemButtonClicked()
{
	if(!MainModule || !ProcedureModule || SelectedProcedureListItems.Num() == 0 || SelectedProcedureListItems[0]->GetProcedureIndex() == 0) return FReply::Unhandled();
	
	if(SelectedProcedureListItems[0]->GetParentProcedure())
	{
		const auto TmpProcedure = SelectedProcedureListItems[0]->GetParentSubProcedures()[SelectedProcedureListItems[0]->GetProcedureIndex()];
		SelectedProcedureListItems[0]->GetParentSubProcedures().RemoveAt(SelectedProcedureListItems[0]->GetProcedureIndex());
		SelectedProcedureListItems[0]->GetParentSubProcedures().Insert(TmpProcedure, SelectedProcedureListItems[0]->GetProcedureIndex() - 1);

		SelectedProcedureListItems[0]->GetParentSubListItems().RemoveAt(SelectedProcedureListItems[0]->GetProcedureIndex());
		SelectedProcedureListItems[0]->GetParentSubListItems().Insert(SelectedProcedureListItems, SelectedProcedureListItems[0]->GetProcedureIndex() - 1);

		SelectedProcedureListItems[0]->GetParentProcedure()->Modify();

		UpdateTreeView();
	}
	else
	{
		const auto TmpProcedure = ProcedureModule->RootProcedures[SelectedProcedureListItems[0]->GetProcedureIndex()];
		ProcedureModule->RootProcedures.RemoveAt(SelectedProcedureListItems[0]->GetProcedureIndex());
		ProcedureModule->RootProcedures.Insert(TmpProcedure, SelectedProcedureListItems[0]->GetProcedureIndex() - 1);

		ProcedureListItems.RemoveAt(SelectedProcedureListItems[0]->GetProcedureIndex());
		ProcedureListItems.Insert(SelectedProcedureListItems, SelectedProcedureListItems[0]->GetProcedureIndex() - 1);

		ProcedureModule->Modify();

		UpdateTreeView();
	}
	
	return FReply::Handled();
}

FReply SProcedureListWidget::OnMoveDownProcedureItemButtonClicked()
{
	if(!MainModule || !ProcedureModule || SelectedProcedureListItems.Num() == 0) return FReply::Unhandled();

	if(SelectedProcedureListItems[0]->GetParentProcedure())
	{
		if(SelectedProcedureListItems[0]->GetProcedureIndex() < SelectedProcedureListItems[0]->GetParentSubProcedures().Num() - 1)
		{
			const auto TmpProcedure = SelectedProcedureListItems[0]->GetParentSubProcedures()[SelectedProcedureListItems[0]->GetProcedureIndex()];
			SelectedProcedureListItems[0]->GetParentSubProcedures().RemoveAt(SelectedProcedureListItems[0]->GetProcedureIndex());
			SelectedProcedureListItems[0]->GetParentSubProcedures().Insert(TmpProcedure, SelectedProcedureListItems[0]->GetProcedureIndex() + 1);

			SelectedProcedureListItems[0]->GetParentSubListItems().RemoveAt(SelectedProcedureListItems[0]->GetProcedureIndex());
			SelectedProcedureListItems[0]->GetParentSubListItems().Insert(SelectedProcedureListItems, SelectedProcedureListItems[0]->GetProcedureIndex() + 1);

			SelectedProcedureListItems[0]->GetParentProcedure()->Modify();

			UpdateTreeView();
		}
		else return FReply::Unhandled();
	}
	else
	{
		if(SelectedProcedureListItems[0]->GetProcedureIndex() < ProcedureModule->RootProcedures.Num() - 1)
		{
			const auto TmpProcedure = ProcedureModule->RootProcedures[SelectedProcedureListItems[0]->GetProcedureIndex()];
			ProcedureModule->RootProcedures.RemoveAt(SelectedProcedureListItems[0]->GetProcedureIndex());
			ProcedureModule->RootProcedures.Insert(TmpProcedure, SelectedProcedureListItems[0]->GetProcedureIndex() + 1);

			ProcedureListItems.RemoveAt(SelectedProcedureListItems[0]->GetProcedureIndex());
			ProcedureListItems.Insert(SelectedProcedureListItems, SelectedProcedureListItems[0]->GetProcedureIndex() + 1);

			ProcedureModule->Modify();

			UpdateTreeView();
		}
		else return FReply::Unhandled();
	}

	return FReply::Handled();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
