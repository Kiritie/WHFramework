// Fill out your copyright notice in the Description page of Project Settings.

#include "Asset/Slate/SAssetModifierEditorWidget.h"

#include "ContentBrowserModule.h"
#include "LevelEditorActions.h"
#include "SAssetView.h"
#include "Asset/Modifier/AssetModifierBase.h"
#include "Main/Slate/SModuleListWidget.h"

#define LOCTEXT_NAMESPACE "AssetEditorWidget"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

const FName SAssetModifierEditorWidget::WidgetName = FName("AssetModifierEditorWidget");

SAssetModifierEditorWidget::SAssetModifierEditorWidget()
{
}

void SAssetModifierEditorWidget::Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& InNomadTab)
{
	SMainEditorWidgetBase::Construct(SMainEditorWidgetBase::FArguments(), InNomadTab);
}

void SAssetModifierEditorWidget::OnCreate()
{
	SMainEditorWidgetBase::OnCreate();
}

void SAssetModifierEditorWidget::OnSave()
{
	SMainEditorWidgetBase::OnSave();

	FLevelEditorActionCallbacks::Save();
}

void SAssetModifierEditorWidget::OnReset()
{
	SMainEditorWidgetBase::OnReset();
}

void SAssetModifierEditorWidget::OnRefresh()
{
	SMainEditorWidgetBase::OnRefresh();
}

void SAssetModifierEditorWidget::OnDestroy()
{
	SMainEditorWidgetBase::OnDestroy();
}

void SAssetModifierEditorWidget::OnBindCommands(const TSharedRef<FUICommandList>& InCommands)
{
	SMainEditorWidgetBase::OnBindCommands(InCommands);
}

void SAssetModifierEditorWidget::RegisterMenuBar(FMenuBarBuilder& InMenuBarBuilder)
{
}

TSharedRef<SWidget> SAssetModifierEditorWidget::CreateDefaultWidget()
{
	ClassViewerOptions.bShowBackgroundBorder = false;
	ClassViewerOptions.bShowUnloadedBlueprints = true;
	ClassViewerOptions.bShowNoneOption = false;

	ClassViewerOptions.bIsBlueprintBaseOnly = false;
	ClassViewerOptions.bIsPlaceableOnly = false;
	ClassViewerOptions.NameTypeToDisplay = EClassViewerNameTypeToDisplay::ClassName;
	ClassViewerOptions.DisplayMode = EClassViewerDisplayMode::ListView;
	ClassViewerOptions.bAllowViewOptions = true;

	ModifierClassFilter = MakeShareable(new FAssetModifierClassFilter);
	
	ClassViewerOptions.ClassFilters.Add(ModifierClassFilter.ToSharedRef());
	
	SAssignNew(ClassPickButton, SComboButton)
		.OnGetMenuContent(this, &SAssetModifierEditorWidget::GenerateClassPicker)
		.ContentPadding(FMargin(2.0f, 2.0f))
		.ToolTipText(this, &SAssetModifierEditorWidget::GetPickedClassName)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &SAssetModifierEditorWidget::GetPickedClassName)
		];

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(FName("ContentBrowser"));
	
	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.GetCurrentSelectionDelegates.Add(&GetCurrentSelectionDelegate);
	AssetPickerConfig.OnGetAssetContextMenu = FOnGetAssetContextMenu::CreateSP(this, &SAssetModifierEditorWidget::OnGetAssetContextMenu);
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SAssetModifierEditorWidget::OnAssetSelected);
	AssetPickerConfig.OnAssetDoubleClicked = FOnAssetSelected::CreateSP(this, &SAssetModifierEditorWidget::OnAssetDoubleClicked);
	AssetPickerConfig.OnAssetEnterPressed = FOnAssetEnterPressed::CreateSP(this, &SAssetModifierEditorWidget::OnAssetEnterPressed);
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
	AssetPickerConfig.bAllowNullSelection = false;
	AssetPickerConfig.bShowBottomToolbar = true;
	AssetPickerConfig.bAutohideSearchBar = false;
	AssetPickerConfig.bCanShowClasses = false;
	AssetPickerConfig.bAddFilterUI = true;
	AssetPickerConfig.SaveSettingsName = TEXT("GlobalAssetPicker");

	return SNew(SVerticalBox)
				
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 8.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			.Padding(10.f, 0.f, 0.f, 0.f)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Modifier Class:")))
			]

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			.Padding(5.f, 0.f, 0.f, 0.f)
			.FillWidth(1.f)
			[
				ClassPickButton.ToSharedRef()
			]

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			.AutoWidth()
			.Padding(5.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.ContentPadding(FMargin(2.f, 2.f))
				.HAlign(HAlign_Center)
				.Text(FText::FromString(TEXT("Modify For All Asset")))
				.OnClicked(this, &SAssetModifierEditorWidget::OnModifyForAllAssetButtonClicked)
				.IsEnabled_Lambda([this](){ return ActivatedAssets.Num() > 0; })
			]

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			.AutoWidth()
			.Padding(5.f, 0.f, 10.f, 0.f)
			[
				SNew(SButton)
				.ContentPadding(FMargin(2.f, 2.f))
				.HAlign(HAlign_Center)
				.Text(FText::FromString(TEXT("Modify For Selected Asset")))
				.OnClicked(this, &SAssetModifierEditorWidget::OnModifyForSelectedAssetButtonClicked)
				.IsEnabled_Lambda([this](){ return SelectedAssets.Num() > 0 && SelectedModifierClass; })
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		];
}

FReply SAssetModifierEditorWidget::OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		RequestCloseAssetModifierEditor();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

TSharedRef<SWidget> SAssetModifierEditorWidget::GenerateClassPicker()
{
	FOnClassPicked OnPicked(FOnClassPicked::CreateRaw(this, &SAssetModifierEditorWidget::OnClassPicked));

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

void SAssetModifierEditorWidget::OnClassPicked(UClass* InClass)
{
	SelectedModifierClass = InClass;
	
	ClassPickButton->SetIsOpen(false);
}

FText SAssetModifierEditorWidget::GetPickedClassName() const
{
	FString ClassName = SelectedModifierClass ? SelectedModifierClass->GetName() : TEXT("None");
	ClassName.RemoveFromEnd(TEXT("_C"));
	return FText::FromString(ClassName);
}

TSharedPtr<SWidget> SAssetModifierEditorWidget::OnGetAssetContextMenu(const TArray<FAssetData>& _SelectedAssets) const
{
	if (_SelectedAssets.Num() <= 0)
	{
		return nullptr;
	}
	
	FMenuBuilder MenuBuilder(true, MakeShared<FUICommandList>());

	MenuBuilder.BeginSection(TEXT("Common"), LOCTEXT("CommonSectionLabel", "Common"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("EditAsset", "Edit..."),
			LOCTEXT("EditAssetTooltip", "Opens the selected item(s) for edit."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit"),
			FUIAction(
				FExecuteAction::CreateLambda([_SelectedAssets] ()
				{
					for(auto& Iter : _SelectedAssets)
					{
						if (UObject* ObjectToEdit = Iter.GetAsset())
						{
							GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(ObjectToEdit);
						}
					}
				}),
				FCanExecuteAction::CreateLambda([] () { return true; })
			)
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(TEXT("Asset"), LOCTEXT("AssetSectionLabel", "Asset"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("BrowseAsset", "Browse to Asset"),
			LOCTEXT("BrowseAssetTooltip", "Browses to the associated asset and selects it in the most recently used Content Browser (summoning one if necessary)"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "SystemWideCommands.FindInContentBrowser.Small"),
			FUIAction(
				FExecuteAction::CreateLambda([_SelectedAssets] ()
				{
					const FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
					ContentBrowserModule.Get().SyncBrowserToAssets(_SelectedAssets);
				}),
				FCanExecuteAction::CreateLambda([] () { return true; })
			)
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SAssetModifierEditorWidget::OnAssetSelected(const FAssetData& AssetData)
{
	SelectedAssets = GetCurrentSelectionDelegate.Execute();
}

void SAssetModifierEditorWidget::OnAssetDoubleClicked(const FAssetData& AssetData)
{
	if (UObject* ObjectToEdit = AssetData.GetAsset())
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(ObjectToEdit);
	}
	RequestCloseAssetModifierEditor();
}

void SAssetModifierEditorWidget::OnAssetEnterPressed(const TArray<FAssetData>& AssetDatas)
{
	for(auto& Iter : AssetDatas)
	{
		if (UObject* ObjectToEdit = Iter.GetAsset())
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(ObjectToEdit);
		}
	}
	RequestCloseAssetModifierEditor();
}

FReply SAssetModifierEditorWidget::OnModifyForAllAssetButtonClicked()
{
	if(UAssetModifierBase* AssetModifier = NewObject<UAssetModifierBase>(GetTransientPackage(), SelectedModifierClass))
	{
		for(auto& Iter : ActivatedAssets)
		{
			if(AssetModifier->CanModify(Iter))
			{
				AssetModifier->DoModify(Iter);
			}
		}
	}
	return FReply::Handled();
}

FReply SAssetModifierEditorWidget::OnModifyForSelectedAssetButtonClicked()
{
	if(UAssetModifierBase* AssetModifier = NewObject<UAssetModifierBase>(GetTransientPackage(), SelectedModifierClass))
	{
		AssetModifier->AddToRoot();
		for(auto& Iter : SelectedAssets)
		{
			if(AssetModifier->CanModify(Iter))
			{
				AssetModifier->DoModify(Iter);
			}
		}
		AssetModifier->RemoveFromRoot();
	}
	return FReply::Handled();
}

void SAssetModifierEditorWidget::RequestCloseAssetModifierEditor()
{
	if (TSharedPtr<SDockTab> AssetPickerTab = FGlobalTabmanager::Get()->FindExistingLiveTab(FTabId("AssetModifierEditor")))
	{
		AssetPickerTab->RequestCloseTab();
	}
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
