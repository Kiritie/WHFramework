// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Widget/SProcedureListWidget.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "SlateOptMacros.h"
#include "SourceCodeNavigation.h"
#include "Common/CommonStatics.h"
#include "Common/Blueprint/Widget/SCreateBlueprintAssetDialog.h"
#include "Procedure/ProcedureBlueprintFactory.h"
#include "Procedure/ProcedureEditorTypes.h"
#include "Procedure/ProcedureModule.h"
#include "Procedure/Base/ProcedureBlueprint.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Procedure/Widget/SProcedureListItemWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SProcedureListWidget::SProcedureListWidget()
{
	WidgetName = FName("ProcedureListWidget");
	WidgetType = EEditorWidgetType::Child;

	bMultiMode = true;
	bEditMode = false;

	ProcedureModule = nullptr;
}

void SProcedureListWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());

	ProcedureModule = InArgs._ProcedureModule;

	if(!ProcedureModule) return;

	if(!ListView.IsValid())
	{
		SAssignNew(ListView, SListView< TSharedPtr<FProcedureListItem> >)
			.ListItemsSource(&ProcedureListItems)
			.OnGenerateRow(this, &SProcedureListWidget::GenerateListRow)
			.OnItemScrolledIntoView(this, &SProcedureListWidget::ListItemScrolledIntoView)
			.ItemHeight(18)
			.SelectionMode_Lambda([this](){ return bMultiMode ? ESelectionMode::Multi : ESelectionMode::SingleToggle; })
			.OnSelectionChanged(this, &SProcedureListWidget::ListSelectionChanged)
			.ClearSelectionOnClick(false);
	}

	if(!ScrollBar.IsValid())
	{
		SAssignNew(ScrollBar, SScrollBar)
			.Thickness(FVector2D(9.0f, 9.0f));
	}

	SelectedProcedureClass = nullptr;

	ClassViewerOptions.bShowBackgroundBorder = false;
	ClassViewerOptions.bShowUnloadedBlueprints = true;
	ClassViewerOptions.bShowNoneOption = false;

	ClassViewerOptions.bIsBlueprintBaseOnly = false;
	ClassViewerOptions.bIsPlaceableOnly = false;
	ClassViewerOptions.NameTypeToDisplay = EClassViewerNameTypeToDisplay::ClassName;
	ClassViewerOptions.DisplayMode = EClassViewerDisplayMode::ListView;
	ClassViewerOptions.bAllowViewOptions = true;

	ProcedureClassFilter = MakeShareable(new FProcedureClassFilter);
	ProcedureClassFilter->IncludeParentClass = UProcedureBase::StaticClass();
	ProcedureClassFilter->ProcedureModule = ProcedureModule;
	#if ENGINE_MAJOR_VERSION == 4
	ClassViewerOptions.ClassFilter = ProcedureClassFilter;
	#else if ENGINE_MAJOR_VERSION == 5
	ClassViewerOptions.ClassFilters.Add(ProcedureClassFilter.ToSharedRef());
	#endif

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
						.OnClicked(this, &SProcedureListWidget::OnNewProcedureClassButtonClicked)
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
						.IsEnabled_Lambda([this](){ return SelectedProcedureClass != nullptr; })
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
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Add")))
						.IsEnabled_Lambda([this](){ return !bEditMode && SelectedProcedureClass; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SProcedureListWidget::OnAddProcedureItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Insert")))
						.IsEnabled_Lambda([this](){ return !bEditMode && SelectedProcedureListItems.Num() == 1 && SelectedProcedureClass; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SProcedureListWidget::OnInsertProcedureItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Append")))
						.IsEnabled_Lambda([this](){ return !bEditMode && SelectedProcedureListItems.Num() == 1 && SelectedProcedureClass; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SProcedureListWidget::OnAppendProcedureItemButtonClicked)
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
						ListView.ToSharedRef()
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
						.Text(FText::FromString(TEXT("Clear All")))
						.IsEnabled_Lambda([this](){ return ProcedureModule->GetProcedures().Num() > 0; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SProcedureListWidget::OnClearAllProcedureItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Remove")))
						.IsEnabled_Lambda([this](){ return SelectedProcedureListItems.Num() > 0; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SProcedureListWidget::OnRemoveProcedureItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Move Up")))
						.IsEnabled_Lambda([this](){ return SelectedProcedureListItems.Num() == 1 && SelectedProcedureListItems[0]->GetProcedureIndex() > 0; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SProcedureListWidget::OnMoveUpProcedureItemButtonClicked)
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
							return SelectedProcedureListItems.Num() == 1 &&
								SelectedProcedureListItems[0]->GetProcedureIndex() < ProcedureModule->GetProcedures().Num() - 1;
						})
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SProcedureListWidget::OnMoveDownProcedureItemButtonClicked)
					]
				]
			]
		]
	];

	GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.ProcedureEditorSettings"), TEXT("bMultiMode"), bMultiMode, GProcedureEditorIni);
	GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.ProcedureEditorSettings"), TEXT("bEditMode"), bEditMode, GProcedureEditorIni);

	UpdateListView(true);

	SetIsMultiMode(bMultiMode);
	SetIsEditMode(bEditMode);
}

void SProcedureListWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();
}

void SProcedureListWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void SProcedureListWidget::OnRefresh()
{
	SEditorWidgetBase::OnRefresh();

	ProcedureClassFilter->ProcedureModule = ProcedureModule;

	UpdateListView(true);
	UpdateSelection();
	SetIsEditMode(bEditMode);
}

void SProcedureListWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

UProcedureBase* SProcedureListWidget::GenerateProcedure(TSubclassOf<UProcedureBase> InClass)
{
	UProcedureBase* NewProcedure = NewObject<UProcedureBase>(ProcedureModule, InClass, NAME_None);

	// NewProcedure->ProcedureName = *CurrentProcedureClass->GetName().Replace(TEXT("Procedure_"), TEXT(""));
	// NewProcedure->ProcedureDisplayName = FText::FromName(NewProcedure->ProcedureName);

	NewProcedure->OnGenerate();

	return NewProcedure;
}

UProcedureBase* SProcedureListWidget::DuplicateProcedure(UProcedureBase* InProcedure)
{
	UProcedureBase* NewProcedure = DuplicateObject<UProcedureBase>(InProcedure, ProcedureModule, NAME_None);

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
			+ SVerticalBox::Slot()
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

	if(!bEditMode)
	{
		SelectedProcedureClass = InClass;
	}
	else if(SelectedProcedureListItems.Num() > 0)
	{
		if(FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("Are you sure to change selected procedures class?"))) == EAppReturnType::Yes)
		{
			for(int32 i = 0; i < SelectedProcedureListItems.Num(); i++)
			{
				UProcedureBase* OldProcedure = SelectedProcedureListItems[i]->Procedure;
				UProcedureBase* NewProcedure = GenerateProcedure(InClass);

				if(NewProcedure && OldProcedure)
				{
					UCommonStatics::ExportPropertiesToObject(OldProcedure, NewProcedure);
					ProcedureModule->GetProcedures().EmplaceAt(OldProcedure->ProcedureIndex, NewProcedure);
					ProcedureModule->GetProcedureMap().Remove(OldProcedure->GetClass());
					ProcedureModule->GetProcedureMap().Add(OldProcedure->GetClass(), OldProcedure);
					OldProcedure->OnUnGenerate();
					Refresh();
				}
			}

			ProcedureModule->Modify();

			OnSelectProcedureListItemsDelegate.Execute(SelectedProcedureListItems);
			SelectedProcedureClass = InClass;
		}
	}
}

FText SProcedureListWidget::GetPickedClassName() const
{
	return FText::FromString(SelectedProcedureListItems.Num() <= 1 ? (SelectedProcedureClass ? SelectedProcedureClass->GetName() : TEXT("None")) : TEXT("Multiple Values"));
}

void SProcedureListWidget::ToggleEditMode()
{
	SetIsEditMode(!bEditMode);
}

void SProcedureListWidget::SetIsEditMode(bool bIsEditMode)
{
	bEditMode = bIsEditMode;
	if(bEditMode)
	{
		SelectedProcedureClass = SelectedProcedureListItems.Num() > 0 ? SelectedProcedureListItems[0]->Procedure->GetClass() : nullptr;
	}
	else
	{
		SelectedProcedureClass = nullptr;
	}
	GConfig->SetBool(TEXT("/Script/WHFrameworkEditor.ProcedureEditorSettings"), TEXT("bEditMode"), bEditMode, GProcedureEditorIni);
}

void SProcedureListWidget::ToggleMultiMode()
{
	SetIsMultiMode(!bMultiMode);
}

void SProcedureListWidget::SetIsMultiMode(bool bIsMultiMode)
{
	bMultiMode = bIsMultiMode;
	if(!bMultiMode)
	{
		ListView->ClearSelection();
	}
	GConfig->SetBool(TEXT("/Script/WHFrameworkEditor.ProcedureEditorSettings"), TEXT("bMultiMode"), bMultiMode, GProcedureEditorIni);
}

int32 SProcedureListWidget::GetTotalProcedureNum() const
{
	return ProcedureListItems.Num();
}

int32 SProcedureListWidget::GetSelectedProcedureNum() const
{
	return SelectedProcedureListItems.Num();
}

void SProcedureListWidget::UpdateListView(bool bRegenerate)
{
	if(!ProcedureModule) return;

	if(bRegenerate)
	{
		ProcedureModule->GenerateListItem(ProcedureListItems);
	}
	else
	{
		ProcedureModule->UpdateListItem(ProcedureListItems);
	}

	ListView->ClearSelection();
	ListView->RequestListRefresh();
}

void SProcedureListWidget::UpdateSelection()
{
	SelectedProcedureListItems = ListView->GetSelectedItems();
	
	if(bEditMode)
	{
		SelectedProcedureClass = SelectedProcedureListItems.Num() > 0 ? SelectedProcedureListItems[0]->Procedure->GetClass() : nullptr;
	}

	if(OnSelectProcedureListItemsDelegate.IsBound())
	{
		OnSelectProcedureListItemsDelegate.Execute(SelectedProcedureListItems);
	}
}

TSharedRef<ITableRow> SProcedureListWidget::GenerateListRow(TSharedPtr<FProcedureListItem> ListItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(ListItem.IsValid());

	return SNew(STableRow<TSharedPtr<FProcedureListItem>>, OwnerTable)
	[
		SNew(SProcedureListItemWidget, ListItem)
	];
}

void SProcedureListWidget::ListItemScrolledIntoView(TSharedPtr<FProcedureListItem> ListItem, const TSharedPtr<ITableRow>& Widget) { }

void SProcedureListWidget::ListSelectionChanged(TSharedPtr<FProcedureListItem> ListItem, ESelectInfo::Type SelectInfo)
{
	UpdateSelection();
}

FReply SProcedureListWidget::OnEditProcedureItemButtonClicked()
{
	if(!ProcedureModule) return FReply::Handled();

	if(SelectedProcedureClass->ClassGeneratedBy)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(SelectedProcedureClass->ClassGeneratedBy);
	}
	else
	{
		FSourceCodeNavigation::NavigateToClass(SelectedProcedureClass);
	}
	return FReply::Handled();
}

FReply SProcedureListWidget::OnNewProcedureClassButtonClicked()
{
	IAssetTools* AssetTools = &FModuleManager::GetModuleChecked<FAssetToolsModule>(FName("AssetTools")).Get();

	FString DefaultAssetPath = TEXT("/Game");
	
	GConfig->GetString(TEXT("/Script/WHFrameworkEditor.ProcedureEditorSettings"), TEXT("DefaultAssetPath"), DefaultAssetPath, GProcedureEditorIni);

	UProcedureBlueprintFactory* ProcedureBlueprintFactory = NewObject<UProcedureBlueprintFactory>(GetTransientPackage(), UProcedureBlueprintFactory::StaticClass());

	const TSharedRef<SCreateBlueprintAssetDialog> CreateBlueprintAssetDialog = SNew(SCreateBlueprintAssetDialog)
	.DefaultAssetPath(FText::FromString(DefaultAssetPath))
	.BlueprintFactory(ProcedureBlueprintFactory);

	if(CreateBlueprintAssetDialog->ShowModal() != EAppReturnType::Cancel)
	{
		DefaultAssetPath = CreateBlueprintAssetDialog->GetAssetPath();

		const FString AssetName = CreateBlueprintAssetDialog->GetAssetName();
	
		const FString PackageName = CreateBlueprintAssetDialog->GetPackageName();

		GConfig->SetString(TEXT("/Script/WHFrameworkEditor.ProcedureEditorSettings"), TEXT("DefaultAssetPath"), *DefaultAssetPath, GProcedureEditorIni);
	
		if(UProcedureBlueprint* ProcedureBlueprintAsset = Cast<UProcedureBlueprint>(AssetTools->CreateAsset(AssetName, DefaultAssetPath, UProcedureBlueprint::StaticClass(), ProcedureBlueprintFactory, FName("ContentBrowserNewAsset"))))
		{
			TArray<UObject*> ObjectsToSyncTo;
			ObjectsToSyncTo.Add(ProcedureBlueprintAsset);

			GEditor->SyncBrowserToObjects(ObjectsToSyncTo);

			//SelectedProcedureClass = ProcedureBlueprintAsset->GetClass();
		}
	}

	return FReply::Handled();
}

FReply SProcedureListWidget::OnAddProcedureItemButtonClicked()
{
	if(!ProcedureModule) return FReply::Handled();

	UProcedureBase* NewProcedure = GenerateProcedure(SelectedProcedureClass);

	const auto Item = MakeShared<FProcedureListItem>();
	Item->Procedure = NewProcedure;

	NewProcedure->ProcedureIndex = ProcedureModule->GetProcedures().Num();
	ProcedureModule->GetProcedures().Add(NewProcedure);
	ProcedureModule->GetProcedureMap().Add(NewProcedure->GetClass(), NewProcedure);
	ProcedureListItems.Add(Item);

	ProcedureModule->Modify();
	
	SelectedProcedureClass = nullptr;
	
	ListView->SetSelection(Item);
	UpdateListView();

	return FReply::Handled();
}

FReply SProcedureListWidget::OnInsertProcedureItemButtonClicked()
{
	if(!ProcedureModule) return FReply::Handled();

	if(SelectedProcedureListItems.Num() > 0)
	{
		UProcedureBase* NewProcedure = GenerateProcedure(SelectedProcedureClass);

		const auto Item = MakeShared<FProcedureListItem>();
		Item->Procedure = NewProcedure;

		ProcedureModule->GetProcedures().Insert(NewProcedure, SelectedProcedureListItems[0]->Procedure->ProcedureIndex);
		ProcedureModule->GetProcedureMap().Add(NewProcedure->GetClass(), NewProcedure);
		ProcedureListItems.Insert(Item, SelectedProcedureListItems[0]->Procedure->ProcedureIndex);

		ProcedureModule->Modify();

		SelectedProcedureClass = nullptr;

		ListView->SetSelection(Item);
		UpdateListView();
	}

	return FReply::Handled();
}

FReply SProcedureListWidget::OnAppendProcedureItemButtonClicked()
{
	if(!ProcedureModule) return FReply::Handled();

	if(SelectedProcedureListItems.Num() > 0)
	{
		UProcedureBase* NewProcedure = GenerateProcedure(SelectedProcedureClass);

		const auto Item = MakeShared<FProcedureListItem>();
		Item->Procedure = NewProcedure;

		ProcedureModule->GetProcedures().Insert(NewProcedure, SelectedProcedureListItems[0]->Procedure->ProcedureIndex + 1);
		ProcedureModule->GetProcedureMap().Add(NewProcedure->GetClass(), NewProcedure);
		ProcedureListItems.Insert(Item, SelectedProcedureListItems[0]->Procedure->ProcedureIndex + 1);

		ProcedureModule->Modify();

		SelectedProcedureClass = nullptr;

		ListView->SetSelection(Item);
		UpdateListView();
	}

	return FReply::Handled();
}

FReply SProcedureListWidget::OnRemoveProcedureItemButtonClicked()
{
	if(!ProcedureModule) return FReply::Handled();

	if(FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("Are you sure to remove selected procedures?"))) != EAppReturnType::Yes) return FReply::Handled();

	if(SelectedProcedureListItems.Num() > 0)
	{
		for(auto Iter : SelectedProcedureListItems)
		{
			ProcedureModule->GetProcedures()[Iter->GetProcedureIndex()]->OnUnGenerate();
			ProcedureModule->GetProcedures().RemoveAt(Iter->GetProcedureIndex());
			ProcedureModule->GetProcedureMap().Remove(Iter->Procedure->GetClass());
			ProcedureListItems.RemoveAt(Iter->GetProcedureIndex());
			//ListView->SetSelection(ProcedureListItems[FMath::Min(SelectedProcedureListItem->GetProcedureIndex(),ProcedureListItems.Num() - 1)]);

			ProcedureModule->Modify();
		}
		UpdateListView();
	}

	return FReply::Handled();
}

FReply SProcedureListWidget::OnClearAllProcedureItemButtonClicked()
{
	if(!ProcedureModule) return FReply::Handled();

	if(FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("Are you sure to clear all procedures?"))) != EAppReturnType::Yes) return FReply::Handled();

	ProcedureModule->ClearAllProcedure();

	UpdateListView(true);

	return FReply::Handled();
}

FReply SProcedureListWidget::OnMoveUpProcedureItemButtonClicked()
{
	if(!ProcedureModule || SelectedProcedureListItems.Num() == 0 || SelectedProcedureListItems[0]->GetProcedureIndex() == 0) return FReply::Handled();

	const auto TmpProcedure = ProcedureModule->GetProcedures()[SelectedProcedureListItems[0]->GetProcedureIndex()];
	ProcedureModule->GetProcedures().RemoveAt(SelectedProcedureListItems[0]->GetProcedureIndex());
	ProcedureModule->GetProcedures().Insert(TmpProcedure, SelectedProcedureListItems[0]->GetProcedureIndex() - 1);

	ProcedureListItems.RemoveAt(SelectedProcedureListItems[0]->GetProcedureIndex());
	ProcedureListItems.Insert(SelectedProcedureListItems, SelectedProcedureListItems[0]->GetProcedureIndex() - 1);

	ProcedureModule->Modify();

	UpdateListView();

	return FReply::Handled();
}

FReply SProcedureListWidget::OnMoveDownProcedureItemButtonClicked()
{
	if(!ProcedureModule || SelectedProcedureListItems.Num() == 0) return FReply::Handled();

	if(SelectedProcedureListItems[0]->GetProcedureIndex() < ProcedureModule->GetProcedures().Num() - 1)
	{
		const auto TmpProcedure = ProcedureModule->GetProcedures()[SelectedProcedureListItems[0]->GetProcedureIndex()];
		ProcedureModule->GetProcedures().RemoveAt(SelectedProcedureListItems[0]->GetProcedureIndex());
		ProcedureModule->GetProcedures().Insert(TmpProcedure, SelectedProcedureListItems[0]->GetProcedureIndex() + 1);

		ProcedureListItems.RemoveAt(SelectedProcedureListItems[0]->GetProcedureIndex());
		ProcedureListItems.Insert(SelectedProcedureListItems, SelectedProcedureListItems[0]->GetProcedureIndex() + 1);

		ProcedureModule->Modify();

		UpdateListView();
	}

	return FReply::Handled();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
