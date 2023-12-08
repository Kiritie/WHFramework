// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Widget/SProcedureListWidget.h"

#include "SlateOptMacros.h"
#include "SourceCodeNavigation.h"
#include "Common/CommonEditorStatics.h"
#include "Common/CommonStatics.h"
#include "Procedure/ProcedureEditorTypes.h"
#include "Procedure/Base/ProcedureAsset.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Procedure/Blueprint/ProcedureBlueprintFactory.h"
#include "Procedure/Widget/SProcedureDetailsWidget.h"
#include "Procedure/Widget/SProcedureListItemWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SProcedureListWidget::SProcedureListWidget()
{
	WidgetName = FName("ProcedureListWidget");
	WidgetType = EEditorWidgetType::Child;

	bDefaults = false;
	bEditing = false;
}

void SProcedureListWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());
	
	ProcedureEditor = InArgs._ProcedureEditor;

	if(!ListView.IsValid())
	{
		SAssignNew(ListView, SListView< TSharedPtr<FProcedureListItem> >)
			.ListItemsSource(&ProcedureListItems)
			.OnGenerateRow(this, &SProcedureListWidget::GenerateListRow)
			.OnItemScrolledIntoView(this, &SProcedureListWidget::ListItemScrolledIntoView)
			.ItemHeight(18)
			.SelectionMode(ESelectionMode::Multi)
			.OnSelectionChanged(this, &SProcedureListWidget::ListSelectionChanged)
			.ClearSelectionOnClick(true);
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
	ProcedureClassFilter->ProcedureEditor = ProcedureEditor;
	
	ClassViewerOptions.ClassFilters.Add(ProcedureClassFilter.ToSharedRef());

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
						.IsEnabled_Lambda([this](){ return !bEditing && SelectedProcedureClass; })
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
						.IsEnabled_Lambda([this](){ return !bEditing && SelectedProcedureListItems.Num() == 1 && SelectedProcedureClass; })
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
						.IsEnabled_Lambda([this](){ return !bEditing && SelectedProcedureListItems.Num() == 1 && SelectedProcedureClass; })
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
						.IsEnabled_Lambda([this](){ return ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->GetProcedures().Num() > 0; })
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
								SelectedProcedureListItems[0]->GetProcedureIndex() < ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->GetProcedures().Num() - 1;
						})
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SProcedureListWidget::OnMoveDownProcedureItemButtonClicked)
					]
				]
			]
		]
	];

	GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.ProcedureEditorSettings"), TEXT("bDefaults"), bDefaults, GProcedureEditorIni);
	GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.ProcedureEditorSettings"), TEXT("bEditing"), bEditing, GProcedureEditorIni);

	UpdateListView(true);
}

void SProcedureListWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();
}

void SProcedureListWidget::OnInitialize()
{
	SEditorWidgetBase::OnInitialize();

	SetIsDefaults(bDefaults);
	SetIsEditing(bEditing);
}

void SProcedureListWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void SProcedureListWidget::OnRefresh()
{
	SEditorWidgetBase::OnRefresh();

	UpdateListView(true);
	UpdateSelection();
	SetIsEditing(bEditing);
}

void SProcedureListWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

FReply SProcedureListWidget::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if(InKeyEvent.GetKey() == EKeys::Escape)
	{
		ListView->ClearSelection();
		return FReply::Handled();
	}
	return SEditorWidgetBase::OnKeyDown(MyGeometry, InKeyEvent);
}

UProcedureBase* SProcedureListWidget::GenerateProcedure(TSubclassOf<UProcedureBase> InClass)
{
	UProcedureBase* NewProcedure = NewObject<UProcedureBase>(ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>(), InClass, NAME_None);

	// NewProcedure->ProcedureName = *CurrentProcedureClass->GetName().Replace(TEXT("Procedure_"), TEXT(""));
	// NewProcedure->ProcedureDisplayName = FText::FromName(NewProcedure->ProcedureName);

	NewProcedure->OnGenerate();

	return NewProcedure;
}

UProcedureBase* SProcedureListWidget::DuplicateProcedure(UProcedureBase* InProcedure)
{
	UProcedureBase* NewProcedure = DuplicateObject<UProcedureBase>(InProcedure, ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>(), NAME_None);

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

	if(!bEditing)
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
					ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->GetProcedures()[OldProcedure->ProcedureIndex] = NewProcedure;
					OldProcedure->OnUnGenerate();
				}
			}

			ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->Modify();

			UpdateListView();

			UpdateSelection();
			
			SelectedProcedureClass = InClass;
		}
	}
}

FText SProcedureListWidget::GetPickedClassName() const
{
	FString ClassName = SelectedProcedureClass ? SelectedProcedureClass->GetName() : TEXT("None");
	ClassName.RemoveFromEnd(TEXT("_C"));
	return FText::FromString(ClassName);
}

void SProcedureListWidget::ToggleEditing()
{
	SetIsEditing(!bEditing);
}

void SProcedureListWidget::SetIsEditing(bool bIsEditing)
{
	bEditing = bIsEditing;
	if(bEditing)
	{
		SelectedProcedureClass = SelectedProcedureListItems.Num() > 0 ? SelectedProcedureListItems[0]->Procedure->GetClass() : nullptr;
	}
	else
	{
		SelectedProcedureClass = nullptr;
	}
	GConfig->SetBool(TEXT("/Script/WHFrameworkEditor.ProcedureEditorSettings"), TEXT("bEditing"), bEditing, GProcedureEditorIni);
}

void SProcedureListWidget::ToggleDefaults()
{
	SetIsDefaults(!bDefaults);
}

void SProcedureListWidget::SetIsDefaults(bool bIsDefaults)
{
	bDefaults = bIsDefaults;
	GConfig->SetBool(TEXT("/Script/WHFrameworkEditor.ProcedureEditorSettings"), TEXT("bDefaults"), bDefaults, GProcedureEditorIni);

	ProcedureEditor.Pin()->DetailsWidget->Refresh();
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
	if(!ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()) return;

	if(bRegenerate)
	{
		ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->GenerateProcedureListItem(ProcedureListItems);

		ListView->ClearSelection();
	}
	else
	{
		ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->UpdateProcedureListItem(ProcedureListItems);
	}

	ListView->RequestListRefresh();
}

void SProcedureListWidget::UpdateSelection()
{
	SelectedProcedureListItems = ListView->GetSelectedItems();
	
	if(bEditing)
	{
		SelectedProcedureClass = SelectedProcedureListItems.Num() > 0 ? SelectedProcedureListItems[0]->Procedure->GetClass() : nullptr;
	}

	if(OnSelectProcedureListItemsDelegate.IsBound())
	{
		OnSelectProcedureListItemsDelegate.Execute(SelectedProcedureListItems);
	}

	ProcedureEditor.Pin()->DetailsWidget->Refresh();
}

TSharedRef<ITableRow> SProcedureListWidget::GenerateListRow(TSharedPtr<FProcedureListItem> ListItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(ListItem.IsValid());

	return SNew(SProcedureListItemWidget, OwnerTable)
		.Item(ListItem);
}

void SProcedureListWidget::ListItemScrolledIntoView(TSharedPtr<FProcedureListItem> ListItem, const TSharedPtr<ITableRow>& Widget) { }

void SProcedureListWidget::ListSelectionChanged(TSharedPtr<FProcedureListItem> ListItem, ESelectInfo::Type SelectInfo)
{
	UpdateSelection();
}

FReply SProcedureListWidget::OnEditProcedureItemButtonClicked()
{
	if(!ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()) return FReply::Handled();

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
	if(const UBlueprint* Blueprint = UCommonEditorStatics::CreateBlueprintAssetWithDialog(UProcedureBlueprintFactory::StaticClass()))
	{
		if(!bEditing)
		{
			SelectedProcedureClass = Blueprint->GeneratedClass;
		}
	}

	return FReply::Handled();
}

FReply SProcedureListWidget::OnAddProcedureItemButtonClicked()
{
	if(!ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()) return FReply::Handled();

	UProcedureBase* NewProcedure = GenerateProcedure(SelectedProcedureClass);

	const auto Item = MakeShared<FProcedureListItem>();
	Item->Procedure = NewProcedure;

	NewProcedure->ProcedureIndex = ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->GetProcedures().Num();
	ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->GetProcedures().Add(NewProcedure);
	ProcedureListItems.Add(Item);

	ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->Modify();
	
	SelectedProcedureClass = nullptr;
	
	ListView->SetSelection(Item);
	UpdateListView();

	return FReply::Handled();
}

FReply SProcedureListWidget::OnInsertProcedureItemButtonClicked()
{
	if(!ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()) return FReply::Handled();

	if(SelectedProcedureListItems.Num() > 0)
	{
		UProcedureBase* NewProcedure = GenerateProcedure(SelectedProcedureClass);

		const auto Item = MakeShared<FProcedureListItem>();
		Item->Procedure = NewProcedure;

		ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->GetProcedures().Insert(NewProcedure, SelectedProcedureListItems[0]->Procedure->ProcedureIndex);
		ProcedureListItems.Insert(Item, SelectedProcedureListItems[0]->Procedure->ProcedureIndex);

		ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->Modify();

		SelectedProcedureClass = nullptr;

		ListView->SetSelection(Item);
		UpdateListView();
	}

	return FReply::Handled();
}

FReply SProcedureListWidget::OnAppendProcedureItemButtonClicked()
{
	if(!ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()) return FReply::Handled();

	if(SelectedProcedureListItems.Num() > 0)
	{
		UProcedureBase* NewProcedure = GenerateProcedure(SelectedProcedureClass);

		const auto Item = MakeShared<FProcedureListItem>();
		Item->Procedure = NewProcedure;

		ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->GetProcedures().Insert(NewProcedure, SelectedProcedureListItems[0]->Procedure->ProcedureIndex + 1);
		ProcedureListItems.Insert(Item, SelectedProcedureListItems[0]->Procedure->ProcedureIndex + 1);

		ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->Modify();

		SelectedProcedureClass = nullptr;

		ListView->SetSelection(Item);
		UpdateListView();
	}

	return FReply::Handled();
}

FReply SProcedureListWidget::OnRemoveProcedureItemButtonClicked()
{
	if(!ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()) return FReply::Handled();

	if(FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("Are you sure to remove selected procedures?"))) != EAppReturnType::Yes) return FReply::Handled();

	if(SelectedProcedureListItems.Num() > 0)
	{
		for(auto Iter : SelectedProcedureListItems)
		{
			ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->GetProcedures()[Iter->GetProcedureIndex()]->OnUnGenerate();
			ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->GetProcedures().RemoveAt(Iter->GetProcedureIndex());
			ProcedureListItems.RemoveAt(Iter->GetProcedureIndex());
			//ListView->SetSelection(ProcedureListItems[FMath::Min(SelectedProcedureListItem->GetProcedureIndex(),ProcedureListItems.Num() - 1)]);

			ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->Modify();
		}
		UpdateListView();
	}

	return FReply::Handled();
}

FReply SProcedureListWidget::OnClearAllProcedureItemButtonClicked()
{
	if(!ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()) return FReply::Handled();

	if(FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("Are you sure to clear all procedures?"))) != EAppReturnType::Yes) return FReply::Handled();

	ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->ClearAllProcedure();

	UpdateListView(true);

	return FReply::Handled();
}

FReply SProcedureListWidget::OnMoveUpProcedureItemButtonClicked()
{
	if(!ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>() || SelectedProcedureListItems.Num() == 0 || SelectedProcedureListItems[0]->GetProcedureIndex() == 0) return FReply::Handled();

	const auto TmpProcedure = ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->GetProcedures()[SelectedProcedureListItems[0]->GetProcedureIndex()];
	ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->GetProcedures().RemoveAt(SelectedProcedureListItems[0]->GetProcedureIndex());
	ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->GetProcedures().Insert(TmpProcedure, SelectedProcedureListItems[0]->GetProcedureIndex() - 1);

	ProcedureListItems.RemoveAt(SelectedProcedureListItems[0]->GetProcedureIndex());
	ProcedureListItems.Insert(SelectedProcedureListItems, SelectedProcedureListItems[0]->GetProcedureIndex() - 1);

	ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->Modify();

	UpdateListView();

	return FReply::Handled();
}

FReply SProcedureListWidget::OnMoveDownProcedureItemButtonClicked()
{
	if(!ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>() || SelectedProcedureListItems.Num() == 0) return FReply::Handled();

	if(SelectedProcedureListItems[0]->GetProcedureIndex() < ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->GetProcedures().Num() - 1)
	{
		const auto TmpProcedure = ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->GetProcedures()[SelectedProcedureListItems[0]->GetProcedureIndex()];
		ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->GetProcedures().RemoveAt(SelectedProcedureListItems[0]->GetProcedureIndex());
		ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->GetProcedures().Insert(TmpProcedure, SelectedProcedureListItems[0]->GetProcedureIndex() + 1);

		ProcedureListItems.RemoveAt(SelectedProcedureListItems[0]->GetProcedureIndex());
		ProcedureListItems.Insert(SelectedProcedureListItems, SelectedProcedureListItems[0]->GetProcedureIndex() + 1);

		ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->Modify();

		UpdateListView();
	}

	return FReply::Handled();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
