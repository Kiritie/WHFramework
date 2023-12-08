// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/Widget/SModuleListWidget.h"

#include "SlateOptMacros.h"
#include "SourceCodeNavigation.h"
#include "Common/CommonEditorStatics.h"
#include "Common/CommonStatics.h"
#include "Main/MainModule.h"
#include "Main/Base/ModuleBase.h"
#include "Main/Widget/SModuleListItemWidget.h"
#include "Main/Blueprint/ModuleBlueprintFactory.h"
#include "Main/Widget/SModuleDetailsWidget.h"
#include "Main/Widget/SModuleEditorWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SModuleListWidget::SModuleListWidget()
{
	WidgetName = FName("ModuleListWidget");
	WidgetType = EEditorWidgetType::Child;

	bDefaults = false;
	bEditing = false;
}

void SModuleListWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());
}

void SModuleListWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();

	if(!ListView.IsValid())
	{
		SAssignNew(ListView, SListView< TSharedPtr<FModuleListItem> >)
			.ListItemsSource(&ModuleListItems)
			.OnGenerateRow(this, &SModuleListWidget::GenerateListRow)
			.OnItemScrolledIntoView(this, &SModuleListWidget::ListItemScrolledIntoView)
			.ItemHeight(18)
			.SelectionMode(ESelectionMode::Multi)
			.OnSelectionChanged(this, &SModuleListWidget::ListSelectionChanged)
			.ClearSelectionOnClick(true);
	}

	if(!ScrollBar.IsValid())
	{
		SAssignNew(ScrollBar, SScrollBar)
			.Thickness(FVector2D(9.0f, 9.0f));
	}

	SelectedModuleClass = nullptr;

	ClassViewerOptions.bShowBackgroundBorder = false;
	ClassViewerOptions.bShowUnloadedBlueprints = true;
	ClassViewerOptions.bShowNoneOption = false;

	ClassViewerOptions.bIsBlueprintBaseOnly = false;
	ClassViewerOptions.bIsPlaceableOnly = false;
	ClassViewerOptions.NameTypeToDisplay = EClassViewerNameTypeToDisplay::ClassName;
	ClassViewerOptions.DisplayMode = EClassViewerDisplayMode::ListView;
	ClassViewerOptions.bAllowViewOptions = true;

	ModuleClassFilter = MakeShareable(new FModuleClassFilter);
	ModuleClassFilter->MainModule = GetParentWidgetN<SModuleEditorWidget>()->MainModule;
	
	ClassViewerOptions.ClassFilters.Add(ModuleClassFilter.ToSharedRef());

	SAssignNew(ClassPickButton, SComboButton)
		.OnGetMenuContent(this, &SModuleListWidget::GenerateClassPicker)
		.ContentPadding(FMargin(2.0f, 2.0f))
		.ToolTipText(this, &SModuleListWidget::GetPickedClassName)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &SModuleListWidget::GetPickedClassName)
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
						.OnClicked(this, &SModuleListWidget::OnNewModuleClassButtonClicked)
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
						.IsEnabled_Lambda([this](){ return SelectedModuleClass != nullptr; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SModuleListWidget::OnEditModuleItemButtonClicked)
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
						.IsEnabled_Lambda([this](){ return !bEditing && SelectedModuleClass; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SModuleListWidget::OnAddModuleItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Add All")))
						.IsEnabled_Lambda([this](){ return !bEditing && GetUnAddedModuleClasses().Num() > 0; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SModuleListWidget::OnAddAllModuleItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Insert")))
						.IsEnabled_Lambda([this](){ return !bEditing && SelectedModuleListItems.Num() == 1 && SelectedModuleClass; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SModuleListWidget::OnInsertModuleItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Append")))
						.IsEnabled_Lambda([this](){ return !bEditing && SelectedModuleListItems.Num() == 1 && SelectedModuleClass; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SModuleListWidget::OnAppendModuleItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Refresh")))
						.IsEnabled_Lambda([this](){ return ModuleListItems.Num() > 0; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SModuleListWidget::OnRefreshModuleItemButtonClicked)
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
						.IsEnabled_Lambda([this](){ return GetParentWidgetN<SModuleEditorWidget>()->MainModule->GetModules().Num() > 0; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SModuleListWidget::OnClearAllModuleItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Remove")))
						.IsEnabled_Lambda([this](){ return SelectedModuleListItems.Num() > 0; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SModuleListWidget::OnRemoveModuleItemButtonClicked)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.ContentPadding(FMargin(0.f, 2.f))
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("Move Up")))
						.IsEnabled_Lambda([this](){ return SelectedModuleListItems.Num() == 1 && SelectedModuleListItems[0]->GetModuleIndex() > 0; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SModuleListWidget::OnMoveUpModuleItemButtonClicked)
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
							return SelectedModuleListItems.Num() == 1 &&
								SelectedModuleListItems[0]->GetModuleIndex() < GetParentWidgetN<SModuleEditorWidget>()->MainModule->GetModules().Num() - 1;
						})
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SModuleListWidget::OnMoveDownModuleItemButtonClicked)
					]
				]
			]
		]
	];

	GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.ModuleEditorSettings"), TEXT("bDefaults"), bDefaults, GModuleEditorIni);
	GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.ModuleEditorSettings"), TEXT("bEditing"), bEditing, GModuleEditorIni);

	UpdateListView(true);
}

void SModuleListWidget::OnInitialize()
{
	SEditorWidgetBase::OnInitialize();

	SetIsDefaults(bDefaults);
	SetIsEditing(bEditing);
}

void SModuleListWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void SModuleListWidget::OnRefresh()
{
	SEditorWidgetBase::OnRefresh();

	ModuleClassFilter->MainModule = GetParentWidgetN<SModuleEditorWidget>()->MainModule;

	UpdateListView(true);
	UpdateSelection();
	SetIsEditing(bEditing);
}

void SModuleListWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

FReply SModuleListWidget::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if(InKeyEvent.GetKey() == EKeys::Escape)
	{
		ListView->ClearSelection();
		return FReply::Handled();
	}
	return SEditorWidgetBase::OnKeyDown(MyGeometry, InKeyEvent);
}

UModuleBase* SModuleListWidget::GenerateModule(TSubclassOf<UModuleBase> InClass)
{
	UModuleBase* NewModule = NewObject<UModuleBase>(GetParentWidgetN<SModuleEditorWidget>()->MainModule, InClass, NAME_None);

	// NewModule->ModuleName = *CurrentModuleClass->GetName().Replace(TEXT("Module_"), TEXT(""));
	// NewModule->ModuleDisplayName = FText::FromName(NewModule->ModuleName);

	NewModule->OnGenerate();

	return NewModule;
}

UModuleBase* SModuleListWidget::DuplicateModule(UModuleBase* InModule)
{
	UModuleBase* NewModule = DuplicateObject<UModuleBase>(InModule, GetParentWidgetN<SModuleEditorWidget>()->MainModule, NAME_None);

	// NewModule->ModuleName = *FString::Printf(TEXT("%s_Copy"), *NewModule->ModuleName.ToString());
	// NewModule->ModuleDisplayName = FText::FromString(FString::Printf(TEXT("%s_Copy"), *NewModule->ModuleDisplayName.ToString()));

	NewModule->OnGenerate();

	return NewModule;
}

TSharedRef<SWidget> SModuleListWidget::GenerateClassPicker()
{
	FOnClassPicked OnPicked(FOnClassPicked::CreateRaw(this, &SModuleListWidget::OnClassPicked));

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

void SModuleListWidget::OnClassPicked(UClass* InClass)
{
	ClassPickButton->SetIsOpen(false);

	if(!bEditing)
	{
		SelectedModuleClass = InClass;
	}
	else if(SelectedModuleListItems.Num() == 1)
	{
		if(FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("Are you sure to change selected module class?"))) == EAppReturnType::Yes)
		{
			UModuleBase* OldModule = SelectedModuleListItems[0]->Module;
			UModuleBase* NewModule = GenerateModule(InClass);

			if(NewModule && OldModule)
			{
				UCommonStatics::ExportPropertiesToObject(OldModule, NewModule);
				GetParentWidgetN<SModuleEditorWidget>()->MainModule->GetModules()[OldModule->GetModuleIndex()] = NewModule;
				GetParentWidgetN<SModuleEditorWidget>()->MainModule->GetModuleMap().Emplace(OldModule->GetModuleName(), NewModule);
				OldModule->OnDestroy();
			}

			GetParentWidgetN<SModuleEditorWidget>()->MainModule->Modify();
			
			UpdateListView();

			UpdateSelection();
		}
	}
}

FText SModuleListWidget::GetPickedClassName() const
{
	FString ClassName = SelectedModuleClass ? SelectedModuleClass->GetName() : TEXT("None");
	ClassName.RemoveFromEnd(TEXT("_C"));
	return FText::FromString(ClassName);
}

void SModuleListWidget::ToggleEditing()
{
	SetIsEditing(!bEditing);
}

void SModuleListWidget::SetIsEditing(bool bIsEditing)
{
	bEditing = bIsEditing;
	if(bEditing)
	{
		SelectedModuleClass = SelectedModuleListItems.Num() > 0 ? SelectedModuleListItems[0]->Module->GetClass() : nullptr;
	}
	else
	{
		SelectedModuleClass = nullptr;
	}
	ModuleClassFilter->EditingModuleClass = SelectedModuleClass;
	
	GConfig->SetBool(TEXT("/Script/WHFrameworkEditor.ModuleEditorSettings"), TEXT("bEditing"), bEditing, GModuleEditorIni);
}

void SModuleListWidget::ToggleDefaults()
{
	SetIsDefaults(!bDefaults);
}

void SModuleListWidget::SetIsDefaults(bool bIsDefaults)
{
	bDefaults = bIsDefaults;
	GConfig->SetBool(TEXT("/Script/WHFrameworkEditor.ModuleEditorSettings"), TEXT("bDefaults"), bDefaults, GModuleEditorIni);

	GetParentWidgetN<SModuleEditorWidget>()->DetailsWidget->Refresh();
}

int32 SModuleListWidget::GetTotalModuleNum() const
{
	return ModuleListItems.Num();
}

int32 SModuleListWidget::GetSelectedModuleNum() const
{
	return SelectedModuleListItems.Num();
}

void SModuleListWidget::UpdateListView(bool bRegenerate)
{
	if(!GetParentWidgetN<SModuleEditorWidget>()->MainModule) return;

	if(bRegenerate)
	{
		GetParentWidgetN<SModuleEditorWidget>()->MainModule->GenerateListItem(ModuleListItems);

		ListView->ClearSelection();
	}
	else
	{
		GetParentWidgetN<SModuleEditorWidget>()->MainModule->UpdateListItem(ModuleListItems);
	}

	ListView->RequestListRefresh();
}

void SModuleListWidget::UpdateSelection()
{
	SelectedModuleListItems = ListView->GetSelectedItems();
	
	if(bEditing)
	{
		SelectedModuleClass = SelectedModuleListItems.Num() > 0 ? SelectedModuleListItems[0]->Module->GetClass() : nullptr;
		ModuleClassFilter->EditingModuleClass = SelectedModuleClass;
	}
	else
	{
		ModuleClassFilter->EditingModuleClass = nullptr;
	}

	if(OnSelectModuleListItemsDelegate.IsBound())
	{
		OnSelectModuleListItemsDelegate.Execute(SelectedModuleListItems);
	}

	GetParentWidgetN<SModuleEditorWidget>()->DetailsWidget->Refresh();
}

TSharedRef<ITableRow> SModuleListWidget::GenerateListRow(TSharedPtr<FModuleListItem> ListItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(ListItem.IsValid());

	return SNew(SModuleListItemWidget, OwnerTable)
		.Item(ListItem);
}

void SModuleListWidget::ListItemScrolledIntoView(TSharedPtr<FModuleListItem> ListItem, const TSharedPtr<ITableRow>& Widget)
{
	
}

void SModuleListWidget::ListSelectionChanged(TSharedPtr<FModuleListItem> ListItem, ESelectInfo::Type SelectInfo)
{
	UpdateSelection();
}

TArray<UClass*> SModuleListWidget::GetUnAddedModuleClasses() const
{
	TArray<UClass*> ReturnValues;
	for (auto Iter : UCommonStatics::GetClassChildren(UModuleBase::StaticClass()))
	{
		if (ModuleClassFilter->IsClassAllowed(Iter) && !FKismetEditorUtilities::IsClassABlueprintSkeleton(Iter) && !UCommonStatics::IsClassHasChildren(Iter))
		{
			ReturnValues.Add(Iter);
		}
	}
	return ReturnValues;
}

FReply SModuleListWidget::OnEditModuleItemButtonClicked()
{
	if(!GetParentWidgetN<SModuleEditorWidget>()->MainModule) return FReply::Handled();

	if(SelectedModuleClass->ClassGeneratedBy)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(SelectedModuleClass->ClassGeneratedBy);
	}
	else
	{
		FSourceCodeNavigation::NavigateToClass(SelectedModuleClass);
	}
	return FReply::Handled();
}

FReply SModuleListWidget::OnNewModuleClassButtonClicked()
{
	if(const UBlueprint* Blueprint = UCommonEditorStatics::CreateBlueprintAssetWithDialog(UModuleBlueprintFactory::StaticClass()))
	{
		if(!bEditing)
		{
			// SelectedModuleClass = Blueprint->GeneratedClass;
		}
	}

	return FReply::Handled();
}

FReply SModuleListWidget::OnAddModuleItemButtonClicked()
{
	if(!GetParentWidgetN<SModuleEditorWidget>()->MainModule) return FReply::Handled();

	UModuleBase* NewModule = GenerateModule(SelectedModuleClass);

	const auto Item = MakeShared<FModuleListItem>();
	Item->Module = NewModule;

	GetParentWidgetN<SModuleEditorWidget>()->MainModule->GetModules().Add(NewModule);
	GetParentWidgetN<SModuleEditorWidget>()->MainModule->GetModuleMap().Add(NewModule->GetModuleName(), NewModule);
	ModuleListItems.Add(Item);

	GetParentWidgetN<SModuleEditorWidget>()->MainModule->Modify();
	
	SelectedModuleClass = nullptr;
	
	ListView->SetSelection(Item);
	UpdateListView();

	return FReply::Handled();
}

FReply SModuleListWidget::OnAddAllModuleItemButtonClicked()
{
	if(!GetParentWidgetN<SModuleEditorWidget>()->MainModule) return FReply::Handled();

	for (auto Iter : GetUnAddedModuleClasses())
	{
		UModuleBase* NewModule = GenerateModule(Iter);

		const auto Item = MakeShared<FModuleListItem>();
		Item->Module = NewModule;

		GetParentWidgetN<SModuleEditorWidget>()->MainModule->GetModules().Add(NewModule);
		GetParentWidgetN<SModuleEditorWidget>()->MainModule->GetModuleMap().Add(NewModule->GetModuleName(), NewModule);
		ModuleListItems.Add(Item);
	}

	SelectedModuleClass = nullptr;

	UpdateListView();

	GetParentWidgetN<SModuleEditorWidget>()->MainModule->Modify();

	return FReply::Handled();
}

FReply SModuleListWidget::OnInsertModuleItemButtonClicked()
{
	if(!GetParentWidgetN<SModuleEditorWidget>()->MainModule) return FReply::Handled();

	if(SelectedModuleListItems.Num() > 0)
	{
		UModuleBase* NewModule = GenerateModule(SelectedModuleClass);

		const auto Item = MakeShared<FModuleListItem>();
		Item->Module = NewModule;

		GetParentWidgetN<SModuleEditorWidget>()->MainModule->GetModules().Insert(NewModule, SelectedModuleListItems[0]->GetModuleIndex());
		GetParentWidgetN<SModuleEditorWidget>()->MainModule->GetModuleMap().Add(NewModule->GetModuleName(), NewModule);
		ModuleListItems.Insert(Item, SelectedModuleListItems[0]->GetModuleIndex());

		GetParentWidgetN<SModuleEditorWidget>()->MainModule->Modify();

		SelectedModuleClass = nullptr;

		ListView->SetSelection(Item);
		UpdateListView();
	}

	return FReply::Handled();
}

FReply SModuleListWidget::OnAppendModuleItemButtonClicked()
{
	if(!GetParentWidgetN<SModuleEditorWidget>()->MainModule) return FReply::Handled();

	if(SelectedModuleListItems.Num() > 0)
	{
		UModuleBase* NewModule = GenerateModule(SelectedModuleClass);

		const auto Item = MakeShared<FModuleListItem>();
		Item->Module = NewModule;

		GetParentWidgetN<SModuleEditorWidget>()->MainModule->GetModules().Insert(NewModule, SelectedModuleListItems[0]->GetModuleIndex() + 1);
		GetParentWidgetN<SModuleEditorWidget>()->MainModule->GetModuleMap().Add(NewModule->GetModuleName(), NewModule);
		ModuleListItems.Insert(Item, SelectedModuleListItems[0]->GetModuleIndex() + 1);

		GetParentWidgetN<SModuleEditorWidget>()->MainModule->Modify();

		SelectedModuleClass = nullptr;

		ListView->SetSelection(Item);
		UpdateListView();
	}

	return FReply::Handled();
}

FReply SModuleListWidget::OnRefreshModuleItemButtonClicked()
{
	if(!GetParentWidgetN<SModuleEditorWidget>()->MainModule) return FReply::Handled();

	GetParentWidgetN<SModuleEditorWidget>()->MainModule->GenerateModules();

	return FReply::Handled();
}

FReply SModuleListWidget::OnRemoveModuleItemButtonClicked()
{
	if(!GetParentWidgetN<SModuleEditorWidget>()->MainModule) return FReply::Handled();

	if(FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("Are you sure to remove selected modules?"))) != EAppReturnType::Yes) return FReply::Handled();

	if(SelectedModuleListItems.Num() > 0)
	{
		for(auto Iter : SelectedModuleListItems)
		{
			GetParentWidgetN<SModuleEditorWidget>()->MainModule->GetModules()[Iter->GetModuleIndex()]->OnDestroy();
			GetParentWidgetN<SModuleEditorWidget>()->MainModule->GetModules().RemoveAt(Iter->GetModuleIndex());
			GetParentWidgetN<SModuleEditorWidget>()->MainModule->GetModuleMap().Remove(Iter->Module->GetModuleName());
			ModuleListItems.RemoveAt(Iter->GetModuleIndex());
			//ListView->SetSelection(ModuleListItems[FMath::Min(SelectedModuleListItem->GetModuleIndex(),ModuleListItems.Num() - 1)]);

			GetParentWidgetN<SModuleEditorWidget>()->MainModule->Modify();
		}
		UpdateListView();
	}

	return FReply::Handled();
}

FReply SModuleListWidget::OnClearAllModuleItemButtonClicked()
{
	if(!GetParentWidgetN<SModuleEditorWidget>()->MainModule) return FReply::Handled();

	if(FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("Are you sure to clear all modules?"))) != EAppReturnType::Yes) return FReply::Handled();

	GetParentWidgetN<SModuleEditorWidget>()->MainModule->DestroyModules();

	UpdateListView(true);

	return FReply::Handled();
}

FReply SModuleListWidget::OnMoveUpModuleItemButtonClicked()
{
	if(!GetParentWidgetN<SModuleEditorWidget>()->MainModule || SelectedModuleListItems.Num() == 0 || SelectedModuleListItems[0]->GetModuleIndex() == 0) return FReply::Handled();

	const auto TmpModule = GetParentWidgetN<SModuleEditorWidget>()->MainModule->GetModules()[SelectedModuleListItems[0]->GetModuleIndex()];
	GetParentWidgetN<SModuleEditorWidget>()->MainModule->GetModules().RemoveAt(SelectedModuleListItems[0]->GetModuleIndex());
	GetParentWidgetN<SModuleEditorWidget>()->MainModule->GetModules().Insert(TmpModule, SelectedModuleListItems[0]->GetModuleIndex() - 1);

	ModuleListItems.RemoveAt(SelectedModuleListItems[0]->GetModuleIndex());
	ModuleListItems.Insert(SelectedModuleListItems, SelectedModuleListItems[0]->GetModuleIndex() - 1);

	GetParentWidgetN<SModuleEditorWidget>()->MainModule->Modify();

	UpdateListView();

	return FReply::Handled();
}

FReply SModuleListWidget::OnMoveDownModuleItemButtonClicked()
{
	if(!GetParentWidgetN<SModuleEditorWidget>()->MainModule || SelectedModuleListItems.Num() == 0) return FReply::Handled();

	if(SelectedModuleListItems[0]->GetModuleIndex() < GetParentWidgetN<SModuleEditorWidget>()->MainModule->GetModules().Num() - 1)
	{
		const auto TmpModule = GetParentWidgetN<SModuleEditorWidget>()->MainModule->GetModules()[SelectedModuleListItems[0]->GetModuleIndex()];
		GetParentWidgetN<SModuleEditorWidget>()->MainModule->GetModules().RemoveAt(SelectedModuleListItems[0]->GetModuleIndex());
		GetParentWidgetN<SModuleEditorWidget>()->MainModule->GetModules().Insert(TmpModule, SelectedModuleListItems[0]->GetModuleIndex() + 1);

		ModuleListItems.RemoveAt(SelectedModuleListItems[0]->GetModuleIndex());
		ModuleListItems.Insert(SelectedModuleListItems, SelectedModuleListItems[0]->GetModuleIndex() + 1);

		GetParentWidgetN<SModuleEditorWidget>()->MainModule->Modify();

		UpdateListView();
	}

	return FReply::Handled();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
