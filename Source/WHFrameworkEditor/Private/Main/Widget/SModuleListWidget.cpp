// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/Widget/SModuleListWidget.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "SlateOptMacros.h"
#include "SourceCodeNavigation.h"
#include "Common/CommonStatics.h"
#include "Common/Blueprint/Widget/SCreateBlueprintAssetDialog.h"
#include "Main/ModuleBlueprintFactory.h"
#include "Main/MainModule.h"
#include "Main/Base/ModuleBlueprint.h"
#include "Main/Base/ModuleBase.h"
#include "Main/Widget/SModuleListItemWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SModuleListWidget::SModuleListWidget()
{
	WidgetName = FName("ModuleListWidget");
	WidgetType = EEditorWidgetType::Child;

	bMultiMode = true;
	bEditMode = false;

	MainModule = nullptr;
}

void SModuleListWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());

	MainModule = InArgs._MainModule;

	if(!MainModule) return;

	if(!ListView.IsValid())
	{
		SAssignNew(ListView, SListView< TSharedPtr<FModuleListItem> >)
			.ListItemsSource(&ModuleListItems)
			.OnGenerateRow(this, &SModuleListWidget::GenerateListRow)
			.OnItemScrolledIntoView(this, &SModuleListWidget::ListItemScrolledIntoView)
			.ItemHeight(18)
			.SelectionMode_Lambda([this](){ return bMultiMode ? ESelectionMode::Multi : ESelectionMode::SingleToggle; })
			.OnSelectionChanged(this, &SModuleListWidget::ListSelectionChanged)
			.ClearSelectionOnClick(false);
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
	ModuleClassFilter->IncludeParentClass = UModuleBase::StaticClass();
	ModuleClassFilter->MainModule = MainModule;
	#if ENGINE_MAJOR_VERSION == 4
	ClassViewerOptions.ClassFilter = ModuleClassFilter;
	#else if ENGINE_MAJOR_VERSION == 5
	ClassViewerOptions.ClassFilters.Add(ModuleClassFilter.ToSharedRef());
	#endif

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
						.IsEnabled_Lambda([this](){ return !bEditMode && SelectedModuleClass; })
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
						.IsEnabled_Lambda([this](){ return !bEditMode && GetUnAddedModuleClasses().Num() > 0; })
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
						.IsEnabled_Lambda([this](){ return !bEditMode && SelectedModuleListItems.Num() == 1 && SelectedModuleClass; })
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
						.IsEnabled_Lambda([this](){ return !bEditMode && SelectedModuleListItems.Num() == 1 && SelectedModuleClass; })
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SModuleListWidget::OnAppendModuleItemButtonClicked)
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
						.IsEnabled_Lambda([this](){ return MainModule->GetAllModule().Num() > 0; })
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
								SelectedModuleListItems[0]->GetModuleIndex() < MainModule->GetModules().Num() - 1;
						})
						.ClickMethod(EButtonClickMethod::MouseDown)
						.OnClicked(this, &SModuleListWidget::OnMoveDownModuleItemButtonClicked)
					]
				]
			]
		]
	];

	GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.ModuleEditorSettings"), TEXT("bMultiMode"), bMultiMode, GModuleEditorIni);
	GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.ModuleEditorSettings"), TEXT("bEditMode"), bEditMode, GModuleEditorIni);

	UpdateListView(true);

	SetIsMultiMode(bMultiMode);
	SetIsEditMode(bEditMode);
}

void SModuleListWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();
}

void SModuleListWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void SModuleListWidget::OnRefresh()
{
	SEditorWidgetBase::OnRefresh();

	ModuleClassFilter->MainModule = MainModule;

	UpdateListView(true);
	UpdateSelection();
	SetIsEditMode(bEditMode);
}

void SModuleListWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

UModuleBase* SModuleListWidget::GenerateModule(TSubclassOf<UModuleBase> InClass)
{
	UModuleBase* NewModule = NewObject<UModuleBase>(MainModule, InClass, NAME_None);

	// NewModule->ModuleName = *CurrentModuleClass->GetName().Replace(TEXT("Module_"), TEXT(""));
	// NewModule->ModuleDisplayName = FText::FromName(NewModule->ModuleName);

	NewModule->OnGenerate();

	return NewModule;
}

UModuleBase* SModuleListWidget::DuplicateModule(UModuleBase* InModule)
{
	UModuleBase* NewModule = DuplicateObject<UModuleBase>(InModule, MainModule, NAME_None);

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

	if(!bEditMode)
	{
		SelectedModuleClass = InClass;
	}
	else if(SelectedModuleListItems.Num() > 0)
	{
		if(FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("Are you sure to change selected modules class?"))) == EAppReturnType::Yes)
		{
			for(int32 i = 0; i < SelectedModuleListItems.Num(); i++)
			{
				UModuleBase* OldModule = SelectedModuleListItems[i]->Module;
				UModuleBase* NewModule = GenerateModule(InClass);

				if(NewModule && OldModule)
				{
					UCommonStatics::ExportPropertiesToObject(OldModule, NewModule);
					MainModule->GetModules().EmplaceAt(OldModule->GetModuleIndex(), NewModule);
					MainModule->GetModuleMap().Remove(OldModule->GetModuleName());
					MainModule->GetModuleMap().Add(OldModule->GetModuleName(), OldModule);
					OldModule->OnDestroy();
					Refresh();
				}
			}

			MainModule->Modify();

			OnSelectModuleListItemsDelegate.Execute(SelectedModuleListItems);
			SelectedModuleClass = InClass;
		}
	}
}

FText SModuleListWidget::GetPickedClassName() const
{
	return FText::FromString(SelectedModuleListItems.Num() <= 1 ? (SelectedModuleClass ? SelectedModuleClass->GetName() : TEXT("None")) : TEXT("Multiple Values"));
}

void SModuleListWidget::ToggleEditMode()
{
	SetIsEditMode(!bEditMode);
}

void SModuleListWidget::SetIsEditMode(bool bIsEditMode)
{
	bEditMode = bIsEditMode;
	if(bEditMode)
	{
		SelectedModuleClass = SelectedModuleListItems.Num() > 0 ? SelectedModuleListItems[0]->Module->GetClass() : nullptr;
	}
	else
	{
		SelectedModuleClass = nullptr;
	}
	GConfig->SetBool(TEXT("/Script/WHFrameworkEditor.ModuleEditorSettings"), TEXT("bEditMode"), bEditMode, GModuleEditorIni);
}

void SModuleListWidget::ToggleMultiMode()
{
	SetIsMultiMode(!bMultiMode);
}

void SModuleListWidget::SetIsMultiMode(bool bIsMultiMode)
{
	bMultiMode = bIsMultiMode;
	if(!bMultiMode)
	{
		ListView->ClearSelection();
	}
	GConfig->SetBool(TEXT("/Script/WHFrameworkEditor.ModuleEditorSettings"), TEXT("bMultiMode"), bMultiMode, GModuleEditorIni);
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
	if(!MainModule) return;

	if(bRegenerate)
	{
		MainModule->GenerateListItem(ModuleListItems);
	}
	else
	{
		MainModule->UpdateListItem(ModuleListItems);
	}

	ListView->ClearSelection();
	ListView->RequestListRefresh();
}

void SModuleListWidget::UpdateSelection()
{
	SelectedModuleListItems = ListView->GetSelectedItems();
	
	if(bEditMode)
	{
		SelectedModuleClass = SelectedModuleListItems.Num() > 0 ? SelectedModuleListItems[0]->Module->GetClass() : nullptr;
	}

	if(OnSelectModuleListItemsDelegate.IsBound())
	{
		OnSelectModuleListItemsDelegate.Execute(SelectedModuleListItems);
	}
}

TSharedRef<ITableRow> SModuleListWidget::GenerateListRow(TSharedPtr<FModuleListItem> ListItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(ListItem.IsValid());

	return SNew(STableRow<TSharedPtr<FModuleListItem>>, OwnerTable)
	[
		SNew(SModuleListItemWidget, ListItem)
	];
}

void SModuleListWidget::ListItemScrolledIntoView(TSharedPtr<FModuleListItem> ListItem, const TSharedPtr<ITableRow>& Widget) { }

void SModuleListWidget::ListSelectionChanged(TSharedPtr<FModuleListItem> ListItem, ESelectInfo::Type SelectInfo)
{
	UpdateSelection();
}

TArray<UClass*> SModuleListWidget::GetUnAddedModuleClasses() const
{
	TArray<UClass*> ReturnValues;
	for (auto Iter : UCommonStatics::GetAllChildClasses(UModuleBase::StaticClass()))
	{
		if (!MainModule->GetModuleMap().Contains(Iter->GetDefaultObject<UModuleBase>()->GetModuleName()))
		{
			ReturnValues.Add(Iter);
		}
	}
	return ReturnValues;
}

FReply SModuleListWidget::OnEditModuleItemButtonClicked()
{
	if(!MainModule) return FReply::Handled();

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
	IAssetTools* AssetTools = &FModuleManager::GetModuleChecked<FAssetToolsModule>(FName("AssetTools")).Get();

	FString DefaultAssetPath = TEXT("/Game");
	
	GConfig->GetString(TEXT("/Script/WHFrameworkEditor.ModuleEditorSettings"), TEXT("DefaultAssetPath"), DefaultAssetPath, GModuleEditorIni);

	UModuleBlueprintFactory* ModuleBlueprintFactory = NewObject<UModuleBlueprintFactory>(GetTransientPackage(), UModuleBlueprintFactory::StaticClass());

	const TSharedRef<SCreateBlueprintAssetDialog> CreateBlueprintAssetDialog = SNew(SCreateBlueprintAssetDialog)
	.DefaultAssetPath(FText::FromString(DefaultAssetPath))
	.BlueprintFactory(ModuleBlueprintFactory);

	if(CreateBlueprintAssetDialog->ShowModal() != EAppReturnType::Cancel)
	{
		DefaultAssetPath = CreateBlueprintAssetDialog->GetAssetPath();

		const FString AssetName = CreateBlueprintAssetDialog->GetAssetName();
	
		const FString PackageName = CreateBlueprintAssetDialog->GetPackageName();

		GConfig->SetString(TEXT("/Script/WHFrameworkEditor.ModuleEditorSettings"), TEXT("DefaultAssetPath"), *DefaultAssetPath, GModuleEditorIni);
	
		if(UModuleBlueprint* ModuleBlueprintAsset = Cast<UModuleBlueprint>(AssetTools->CreateAsset(AssetName, DefaultAssetPath, UModuleBlueprint::StaticClass(), ModuleBlueprintFactory, FName("ContentBrowserNewAsset"))))
		{
			TArray<UObject*> ObjectsToSyncTo;
			ObjectsToSyncTo.Add(ModuleBlueprintAsset);

			GEditor->SyncBrowserToObjects(ObjectsToSyncTo);

			//SelectedModuleClass = ModuleBlueprintAsset->GetClass();
		}
	}

	return FReply::Handled();
}

FReply SModuleListWidget::OnAddModuleItemButtonClicked()
{
	if(!MainModule) return FReply::Handled();

	UModuleBase* NewModule = GenerateModule(SelectedModuleClass);

	const auto Item = MakeShared<FModuleListItem>();
	Item->Module = NewModule;

	MainModule->GetModules().Add(NewModule);
	MainModule->GetModuleMap().Add(NewModule->GetModuleName(), NewModule);
	ModuleListItems.Add(Item);

	MainModule->Modify();
	
	SelectedModuleClass = nullptr;
	
	ListView->SetSelection(Item);
	UpdateListView();

	return FReply::Handled();
}

FReply SModuleListWidget::OnAddAllModuleItemButtonClicked()
{
	if(!MainModule) return FReply::Handled();

	for (auto Iter : GetUnAddedModuleClasses())
	{
		UModuleBase* NewModule = GenerateModule(Iter);

		const auto Item = MakeShared<FModuleListItem>();
		Item->Module = NewModule;

		MainModule->GetModules().Add(NewModule);
		MainModule->GetModuleMap().Add(NewModule->GetModuleName(), NewModule);
		ModuleListItems.Add(Item);
	}

	SelectedModuleClass = nullptr;

	UpdateListView();

	MainModule->Modify();

	return FReply::Handled();
}

FReply SModuleListWidget::OnInsertModuleItemButtonClicked()
{
	if(!MainModule) return FReply::Handled();

	if(SelectedModuleListItems.Num() > 0)
	{
		UModuleBase* NewModule = GenerateModule(SelectedModuleClass);

		const auto Item = MakeShared<FModuleListItem>();
		Item->Module = NewModule;

		MainModule->GetModules().Insert(NewModule, SelectedModuleListItems[0]->GetModuleIndex());
		MainModule->GetModuleMap().Add(NewModule->GetModuleName(), NewModule);
		ModuleListItems.Insert(Item, SelectedModuleListItems[0]->GetModuleIndex());

		MainModule->Modify();

		SelectedModuleClass = nullptr;

		ListView->SetSelection(Item);
		UpdateListView();
	}

	return FReply::Handled();
}

FReply SModuleListWidget::OnAppendModuleItemButtonClicked()
{
	if(!MainModule) return FReply::Handled();

	if(SelectedModuleListItems.Num() > 0)
	{
		UModuleBase* NewModule = GenerateModule(SelectedModuleClass);

		const auto Item = MakeShared<FModuleListItem>();
		Item->Module = NewModule;

		MainModule->GetModules().Insert(NewModule, SelectedModuleListItems[0]->GetModuleIndex() + 1);
		MainModule->GetModuleMap().Add(NewModule->GetModuleName(), NewModule);
		ModuleListItems.Insert(Item, SelectedModuleListItems[0]->GetModuleIndex() + 1);

		MainModule->Modify();

		SelectedModuleClass = nullptr;

		ListView->SetSelection(Item);
		UpdateListView();
	}

	return FReply::Handled();
}

FReply SModuleListWidget::OnRemoveModuleItemButtonClicked()
{
	if(!MainModule) return FReply::Handled();

	if(FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("Are you sure to remove selected modules?"))) != EAppReturnType::Yes) return FReply::Handled();

	if(SelectedModuleListItems.Num() > 0)
	{
		for(auto Iter : SelectedModuleListItems)
		{
			MainModule->GetModules()[Iter->GetModuleIndex()]->OnDestroy();
			MainModule->GetModules().RemoveAt(Iter->GetModuleIndex());
			MainModule->GetModuleMap().Remove(Iter->Module->GetModuleName());
			ModuleListItems.RemoveAt(Iter->GetModuleIndex());
			//ListView->SetSelection(ModuleListItems[FMath::Min(SelectedModuleListItem->GetModuleIndex(),ModuleListItems.Num() - 1)]);

			MainModule->Modify();
		}
		UpdateListView();
	}

	return FReply::Handled();
}

FReply SModuleListWidget::OnClearAllModuleItemButtonClicked()
{
	if(!MainModule) return FReply::Handled();

	if(FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("Are you sure to clear all modules?"))) != EAppReturnType::Yes) return FReply::Handled();

	MainModule->DestroyModules();

	UpdateListView(true);

	return FReply::Handled();
}

FReply SModuleListWidget::OnMoveUpModuleItemButtonClicked()
{
	if(!MainModule || SelectedModuleListItems.Num() == 0 || SelectedModuleListItems[0]->GetModuleIndex() == 0) return FReply::Handled();

	const auto TmpModule = MainModule->GetModules()[SelectedModuleListItems[0]->GetModuleIndex()];
	MainModule->GetModules().RemoveAt(SelectedModuleListItems[0]->GetModuleIndex());
	MainModule->GetModules().Insert(TmpModule, SelectedModuleListItems[0]->GetModuleIndex() - 1);

	ModuleListItems.RemoveAt(SelectedModuleListItems[0]->GetModuleIndex());
	ModuleListItems.Insert(SelectedModuleListItems, SelectedModuleListItems[0]->GetModuleIndex() - 1);

	MainModule->Modify();

	UpdateListView();

	return FReply::Handled();
}

FReply SModuleListWidget::OnMoveDownModuleItemButtonClicked()
{
	if(!MainModule || SelectedModuleListItems.Num() == 0) return FReply::Handled();

	if(SelectedModuleListItems[0]->GetModuleIndex() < MainModule->GetModules().Num() - 1)
	{
		const auto TmpModule = MainModule->GetModules()[SelectedModuleListItems[0]->GetModuleIndex()];
		MainModule->GetModules().RemoveAt(SelectedModuleListItems[0]->GetModuleIndex());
		MainModule->GetModules().Insert(TmpModule, SelectedModuleListItems[0]->GetModuleIndex() + 1);

		ModuleListItems.RemoveAt(SelectedModuleListItems[0]->GetModuleIndex());
		ModuleListItems.Insert(SelectedModuleListItems, SelectedModuleListItems[0]->GetModuleIndex() + 1);

		MainModule->Modify();

		UpdateListView();
	}

	return FReply::Handled();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
