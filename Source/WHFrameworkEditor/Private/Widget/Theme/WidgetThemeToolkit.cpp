#include "Widget/Theme/WidgetThemeToolkit.h"
#include "Widget/Theme/WidgetThemeEditorService.h"
#include "Widget/Theme/WidgetThemePreviewButton.h"
#include "Widget/Theme/WidgetTheme.h"
#include "Widget/Theme/WidgetThemeSettings.h"
#include "Widget/Theme/WidgetThemeResolver.h"
#include "Widget/Common/CommonTextBlockN.h"
#include "Widget/Common/CommonImageN.h"
#include "AssetTypeActions_Base.h"
#include "AssetToolsModule.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "SWidgetThemePicker.h"
#include "SWidgetStyleCreationDialog.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Widget/Theme/WidgetThemeEditorSettings.h"
#include "ScopedTransaction.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "UObject/UObjectGlobals.h"
#include "ToolMenus.h"

#include "Widget/Theme/WidgetStyleTags.h"
#include "Styling/AppStyle.h"
#include "WorkspaceMenuStructure.h"

#define LOCTEXT_NAMESPACE "WidgetThemeToolkit"
namespace
{
	const FName TagsTab(TEXT("WidgetTheme.Tags"));
	const FName PreviewTab(TEXT("WidgetTheme.Preview"));
	const FName DetailsTab(TEXT("WidgetTheme.Details"));
	const FName ContextTab(TEXT("WidgetTheme.Context"));
	TSharedPtr<IAssetTypeActions> ThemeActions;
	FDelegateHandle MenuStartupHandle;
	class FThemeAssetActions final : public FAssetTypeActions_Base
	{
	public:
		virtual FText GetName() const override
		{
			return LOCTEXT("AssetName", "Theme Asset");
		}
		virtual FColor GetTypeColor() const override
		{
			return FColor(48, 130, 180);
		}
		virtual UClass* GetSupportedClass() const override
		{
			return UWidgetTheme::StaticClass();
		}
		virtual uint32 GetCategories() override
		{
			return FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().FindAdvancedAssetCategory(TEXT("WHFramework"));
		}
		virtual void OpenAssetEditor(const TArray<UObject*>& Assets, TSharedPtr<IToolkitHost> Host) override
		{
			for (UObject* A : Assets)
			{
				if (auto* T = Cast<UWidgetTheme>(A))
				{
					MakeShared<FWidgetThemeToolkit>()->Open(T, Host);
				}
			}
		}
	};
} // namespace
void RegisterWidgetThemeAssetEditor()
{
	FWidgetThemeEditorService::RegisterAssetLifecycle();
	MenuStartupHandle = UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda(
		[]
		{
			FToolMenuOwnerScoped Owner(TEXT("WidgetThemeEditor"));
			UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
			FToolMenuSection& Section = Menu->FindOrAddSection("WidgetTheme");
			Section.AddMenuEntry("OpenDefaultWidgetTheme", LOCTEXT("OpenDefaultTheme", "Open project widget theme"), FText(), FSlateIcon(),
								 FUIAction(FExecuteAction::CreateLambda(
									 []
									 {
										 if (UWidgetTheme* Default = GetDefault<UWidgetThemeSettings>()->DefaultTheme.LoadSynchronous())
										 {
											 MakeShared<FWidgetThemeToolkit>()->Open(Default, nullptr);
										 }
									 })));
			Section.AddMenuEntry("ShowThemeFolders", LOCTEXT("ShowThemeFolders", "Show Themes folders in Content Browser"), FText(), FSlateIcon(),
								 FUIAction(FExecuteAction::CreateLambda(
											   []
											   {
												   auto* Settings = GetMutableDefault<UWidgetThemeEditorSettings>();
												   Settings->bHideThemeFolders = !Settings->bHideThemeFolders;
												   Settings->SaveConfig();
												   UWidgetThemeEditorSettings::ApplyFolderVisibility();
											   }),
										   FCanExecuteAction(),
										   FIsActionChecked::CreateLambda([] { return !GetDefault<UWidgetThemeEditorSettings>()->bHideThemeFolders; })),
								 EUserInterfaceActionType::ToggleButton);
		}));
	if (!ThemeActions)
	{
		ThemeActions = MakeShared<FThemeAssetActions>();
		FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().RegisterAssetTypeActions(ThemeActions.ToSharedRef());
	}
}
void UnregisterWidgetThemeAssetEditor()
{
	FWidgetThemeEditorService::UnregisterAssetLifecycle();
	UToolMenus::UnRegisterStartupCallback(MenuStartupHandle);
	UToolMenus::UnregisterOwner(TEXT("WidgetThemeEditor"));
	if (ThemeActions && FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get().UnregisterAssetTypeActions(ThemeActions.ToSharedRef());
	}
	ThemeActions.Reset();
}
TSharedRef<FTabManager::FLayout> FWidgetThemeToolkit::CreateDefaultLayout()
{
	return FTabManager::NewLayout("WidgetThemeEditor_DockedNative_v4")
		->AddArea(FTabManager::NewPrimaryArea()
					  ->SetOrientation(Orient_Vertical)
					  ->Split(FTabManager::NewSplitter()
								  ->SetOrientation(Orient_Horizontal)
								  ->SetSizeCoefficient(0.91f)
								  ->Split(FTabManager::NewStack()->SetSizeCoefficient(0.25f)->SetHideTabWell(true)->AddTab(TagsTab, ETabState::OpenedTab))
								  ->Split(FTabManager::NewStack()->SetSizeCoefficient(0.45f)->SetHideTabWell(true)->AddTab(PreviewTab, ETabState::OpenedTab))
								  ->Split(FTabManager::NewStack()->SetSizeCoefficient(0.30f)->SetHideTabWell(true)->AddTab(DetailsTab, ETabState::OpenedTab)))
					  ->Split(FTabManager::NewStack()->SetSizeCoefficient(0.09f)->SetHideTabWell(true)->AddTab(ContextTab, ETabState::OpenedTab)));
}

void FWidgetThemeToolkit::Open(UWidgetTheme* InTheme, const TSharedPtr<IToolkitHost>& Host)
{
	Theme = InTheme;
	PreviewRoot = NewObject<UWidgetTheme>(GetTransientPackage(), NAME_None, RF_Transient);
	FWidgetThemeResolver::SetPreviewOverride(PreviewRoot, Theme);
	PreviewButton = NewObject<UWidgetThemePreviewButton>(PreviewRoot);
	PreviewButton->SetDesignerFlags(EWidgetDesignFlags::Designing);
	PreviewButton->Initialize();
	PreviewButton->SetTitle(LOCTEXT("ButtonPreview", "Text Block"));
	PreviewText = NewObject<UCommonTextBlockN>(PreviewRoot);
	PreviewText->SetDesignerFlags(EWidgetDesignFlags::Designing);
	PreviewText->SetText(LOCTEXT("TextPreview", "Aa 0123 中文"));
	PreviewText->SetJustification(ETextJustify::Center);
	PreviewImage = NewObject<UCommonImageN>(PreviewRoot);
	PreviewImage->SetDesignerFlags(EWidgetDesignFlags::Designing);
	FDetailsViewArgs Args;
	Args.bAllowSearch = true;
	Args.bHideSelectionTip = true;
	Args.NotifyHook = this;
	Details = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(Args);
	Details->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateSP(this, &FWidgetThemeToolkit::CanEditSelection));
	DetailsFinishedHandle = Details->OnFinishedChangingProperties().AddLambda([this](const FPropertyChangedEvent&) { FinishDetailsEdit(); });
	GEditor->RegisterForUndo(this);
	ChangedHandle = FWidgetThemeResolver::OnThemeChanged().AddLambda(
		[this]
		{
			if (!FWidgetThemeEditorService::IsBusy() && !bEditingDetails)
			{
				Refresh();
			}
		});
	PropertyHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddLambda(
		[this](UObject* Object, FPropertyChangedEvent& Event)
		{
			if (FWidgetThemeEditorService::IsBusy() || bEditingDetails)
			{
				return;
			}
			if (Object == Theme)
			{
				Refresh();
			}
			else if (Object && (Object->IsA<UCommonButtonStyle>() || Object->IsA<UCommonTextStyle>() || Object->IsA<UWidgetBrushStyle>()))
			{
				UpdatePreview();
			}
		});
	CompileHandle = GEditor->OnBlueprintCompiled().AddLambda(
		[this]
		{
			if (!FWidgetThemeEditorService::IsBusy() && !bEditingDetails)
			{
				Refresh();
			}
		});
	const auto Layout = CreateDefaultLayout();
	InitAssetEditor(Host.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone, Host, TEXT("WidgetThemeEditor"), Layout, true, true, Theme);
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();
	Extender->AddToolBarExtension(TEXT("Asset"), EExtensionHook::After, GetToolkitCommands(),
								  FToolBarExtensionDelegate::CreateSP(this, &FWidgetThemeToolkit::ExtendToolbar));
	AddToolbarExtender(Extender);
	Refresh();
	RegenerateMenusAndToolbars();
}

FWidgetThemeToolkit::~FWidgetThemeToolkit()
{
	if (Details)
	{
		Details->OnFinishedChangingProperties().Remove(DetailsFinishedHandle);
	}
	FWidgetThemeResolver::OnThemeChanged().Remove(ChangedHandle);
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyHandle);
	if (GEditor)
	{
		GEditor->OnBlueprintCompiled().Remove(CompileHandle);
		GEditor->UnregisterForUndo(this);
	}
	FWidgetThemeResolver::ClearPreviewOverride(PreviewRoot);
}

void FWidgetThemeToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Theme);
	Collector.AddReferencedObject(PreviewRoot);
	Collector.AddReferencedObject(PreviewButton);
	Collector.AddReferencedObject(PreviewText);
	Collector.AddReferencedObject(PreviewImage);
}

void FWidgetThemeToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& Manager)
{
	WorkspaceMenuCategory = Manager->AddLocalWorkspaceMenuCategory(LOCTEXT("Workspace", "Widget Theme"));
	FAssetEditorToolkit::RegisterTabSpawners(Manager);
	Manager->RegisterTabSpawner(TagsTab, FOnSpawnTab::CreateSP(this, &FWidgetThemeToolkit::SpawnTags))
		.SetDisplayName(LOCTEXT("Tags", "Tags"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
	Manager->RegisterTabSpawner(PreviewTab, FOnSpawnTab::CreateSP(this, &FWidgetThemeToolkit::SpawnPreview))
		.SetDisplayName(LOCTEXT("Preview", "Preview"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
	Manager->RegisterTabSpawner(DetailsTab, FOnSpawnTab::CreateSP(this, &FWidgetThemeToolkit::SpawnDetails))
		.SetDisplayName(LOCTEXT("Details", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
	Manager->RegisterTabSpawner(ContextTab, FOnSpawnTab::CreateSP(this, &FWidgetThemeToolkit::SpawnContext))
		.SetDisplayName(LOCTEXT("Context", "Context"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FWidgetThemeToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& Manager)
{
	for (FName Name : {TagsTab, PreviewTab, DetailsTab, ContextTab})
	{
		Manager->UnregisterTabSpawner(Name);
	}
	FAssetEditorToolkit::UnregisterTabSpawners(Manager);
}

void FWidgetThemeToolkit::ExtendToolbar(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateSPLambda(
			this, [this] { Run(TEXT("Validate"), [this](FString& Result) { return FWidgetThemeEditorService::Validate(Theme, Result); }); })),
		NAME_None, LOCTEXT("Validate", "Validate"), LOCTEXT("ValidateHelp", "Validate the current theme"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Check"));
	Builder.AddToolBarButton(
		FUIAction(
			FExecuteAction::CreateSPLambda(
				this, [this] { Run(TEXT("Set default"), [this](FString& Result) { return FWidgetThemeEditorService::SetProjectDefault(Theme, Result); }); }),
			FCanExecuteAction::CreateSPLambda(this, [this] { return Theme && Theme->GetOutermost()->GetName().StartsWith(TEXT("/Game/")); })),
		NAME_None, LOCTEXT("SetDefault", "Set default"), LOCTEXT("DefaultHelp", "Set this theme as the project default"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Star"));
}

bool FWidgetThemeToolkit::CanEditSelection() const
{
	const FWidgetStyleBinding* Binding = Theme ? Theme->Bindings.Find(SelectedTag) : nullptr;
	return Binding && Binding->bManaged && !FWidgetThemeEditorService::IsBusy() && !Theme->bDuplicateFailed;
}

FReply FWidgetThemeToolkit::Run(const FString& OperationName, TFunction<bool(FString&)> Operation)
{
	FinishDetailsEdit();
	FString Result;
	const bool Good = Operation(Result);
	Report += FString::Printf(TEXT("[%s] %s | %s\n%s\n"), Good ? TEXT("OK") : TEXT("Error"), *OperationName, *GetPathNameSafe(Theme), *Result);
	Refresh();
	return FReply::Handled();
}

void FWidgetThemeToolkit::SaveAsset_Execute()
{
	Run(TEXT("Save"), [this](FString& Result) { return FWidgetThemeEditorService::Save(Theme, Result); });
}

FReply FWidgetThemeToolkit::OpenStyleDialog(EWidgetStyleOperation Operation)
{
	FinishDetailsEdit();
	const FString Title = Operation == EWidgetStyleOperation::Create ? TEXT("Create style") : TEXT("Associate style");
	TSharedRef<SWindow> Window = SNew(SWindow).Title(FText::FromString(Title)).ClientSize(FVector2D(620, 420)).SupportsMinimize(false).SupportsMaximize(false);
	Window->SetContent(SNew(SWidgetStyleCreationDialog)
						   .Theme(Theme)
						   .Operation(Operation)
						   .ParentTag(SelectedTag)
						   .OnCompleted(FSimpleDelegate::CreateSPLambda(this,
																		[this]
																		{
																			Report += TEXT("[OK] Style mapped. Save to persist.\n");
																			Refresh();
																		}))
						   .Window(Window));
	TSharedPtr<SWindow> Parent = FSlateApplication::Get().FindBestParentWindowForDialogs(nullptr);
	if (Parent)
	{
		FSlateApplication::Get().AddWindowAsNativeChild(Window, Parent.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(Window);
	}
	return FReply::Handled();
}

FReply FWidgetThemeToolkit::OpenRenameDialog()
{
	if (!CanEditSelection() || WidgetStyleTags::IsRoot(SelectedTag))
	{
		return FReply::Handled();
	}
	const FGameplayTag Tag = SelectedTag;
	FString OriginalName;
	Tag.ToString().Split(TEXT("."), nullptr, &OriginalName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	TSharedRef<SWindow> Window =
		SNew(SWindow).Title(LOCTEXT("RenameStyle", "Rename style")).ClientSize(FVector2D(340, 120)).SupportsMinimize(false).SupportsMaximize(false);
	TSharedRef<SEditableTextBox> Input = SNew(SEditableTextBox)
											 .Text(FText::FromString(OriginalName))
											 .SelectAllTextWhenFocused(true)
											 .OnVerifyTextChanged_Lambda(
												 [](const FText& Text, FText& Error)
												 {
													 FString Reason;
													 const bool Valid = FWidgetThemeEditorService::ValidateName(Text.ToString(), Reason);
													 Error = FText::FromString(Reason);
													 return Valid;
												 });
	TWeakPtr<SWindow> WeakWindow = Window;
	FSimpleDelegate Confirm = FSimpleDelegate::CreateSPLambda(this,
															  [this, Tag, Input, WeakWindow, OriginalName]
															  {
																  FString Reason;
																  const FString Name = Input->GetText().ToString();
																  if (!FWidgetThemeEditorService::ValidateName(Name, Reason) || Name == OriginalName)
																  {
																	  return;
																  }
																  bool Renamed = false;
																  Run(TEXT("Rename"),
																	  [&](FString& Result)
																	  {
																		  Renamed = FWidgetThemeEditorService::RenameStyle(Theme, Tag, Name, Result);
																		  if (!Renamed)
																			  Input->SetError(FText::FromString(Result));
																		  return Renamed;
																	  });
																  if (Renamed)
																  {
																	  if (TSharedPtr<SWindow> Dialog = WeakWindow.Pin())
																		  Dialog->RequestDestroyWindow();
																  }
															  });
	Window->SetContent(SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight().Padding(12, 8)[SNew(STextBlock).Text(LOCTEXT("NewName", "New name"))] +
					   SVerticalBox::Slot().AutoHeight().Padding(12, 0)[Input] +
					   SVerticalBox::Slot()
						   .AutoHeight()
						   .HAlign(HAlign_Right)
						   .Padding(12, 8)[SNew(SButton)
											   .HAlign(HAlign_Center)
											   .Text(LOCTEXT("Confirm", "Confirm"))
											   .IsEnabled_Lambda(
												   [Input, OriginalName]
												   {
													   FString Reason;
													   return Input->GetText().ToString() != OriginalName &&
															  FWidgetThemeEditorService::ValidateName(Input->GetText().ToString(), Reason);
												   })
											   .OnClicked_Lambda(
												   [Confirm]
												   {
													   Confirm.ExecuteIfBound();
													   return FReply::Handled();
												   })]);
	Window->SetWidgetToFocusOnActivate(Input);
	TSharedPtr<SWindow> Parent = FSlateApplication::Get().FindBestParentWindowForDialogs(nullptr);
	if (Parent)
		FSlateApplication::Get().AddWindowAsNativeChild(Window, Parent.ToSharedRef());
	else
		FSlateApplication::Get().AddWindow(Window);
	return FReply::Handled();
}

TSharedRef<SDockTab> FWidgetThemeToolkit::SpawnTags(const FSpawnTabArgs& Args)
{
	auto Action = [this](const TCHAR* Label, TFunction<bool()> Enabled, TFunction<bool(FString&)> Operation) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.HAlign(HAlign_Center)
			.Text(FText::FromString(Label))
			.IsEnabled_Lambda(MoveTemp(Enabled))
			.OnClicked_Lambda([this, Label, Operation] { return Run(Label, Operation); });
	};
	TSharedRef<SHorizontalBox> Types = SNew(SHorizontalBox);
	for (EWidgetStyleKind Type : {EWidgetStyleKind::None, EWidgetStyleKind::Button, EWidgetStyleKind::Text, EWidgetStyleKind::Brush})
	{
		const FString Label = Type == EWidgetStyleKind::None	 ? TEXT("All")
							  : Type == EWidgetStyleKind::Button ? TEXT("Button")
							  : Type == EWidgetStyleKind::Text	 ? TEXT("Text")
																 : TEXT("Brush");
		Types->AddSlot().FillWidth(1).Padding(
			1, 0)[SNew(SCheckBox)
					  .Style(FAppStyle::Get(), "SegmentedCombo.ButtonOnly")
					  .HAlign(HAlign_Center)
					  .Padding(FMargin(2, 3))
					  .IsChecked_Lambda([this, Type] { return TypeFilter == Type ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					  .OnCheckStateChanged_Lambda(
						  [this, Type](ECheckBoxState State)
						  {
							  if (State == ECheckBoxState::Checked)
							  {
								  TypeFilter = Type;
								  Refresh();
							  }
						  })[SNew(STextBlock).Text(FText::FromString(Label)).Justification(ETextJustify::Center)]];
	}
	TSharedRef<SHorizontalBox> Actions = SNew(SHorizontalBox);
	Actions->AddSlot().FillWidth(1)[SNew(SButton)
										.HAlign(HAlign_Center)
										.Text(LOCTEXT("Create", "Create"))
										.OnClicked_Lambda([this] { return OpenStyleDialog(EWidgetStyleOperation::Create); })];
	Actions->AddSlot().FillWidth(1).Padding(4, 0)[SNew(SButton)
													  .HAlign(HAlign_Center)
													  .Text(LOCTEXT("Associate", "Associate"))
													  .OnClicked_Lambda([this] { return OpenStyleDialog(EWidgetStyleOperation::Associate); })];
	Actions->AddSlot().FillWidth(1)[Action(
		TEXT("Remove"), [this] { return bCanRemoveSelection && !FWidgetThemeEditorService::IsBusy(); },
		[this](FString& Result) { return FWidgetThemeEditorService::RemoveBinding(Theme, SelectedTag, Result); })];
	Actions->AddSlot().FillWidth(1).Padding(4, 0)[Action(
		TEXT("Copy"), [this] { return FWidgetThemeEditorService::Blueprint(Theme, SelectedTag) != nullptr; },
		[this](FString& Result) { return FWidgetThemeEditorService::CopyProperties(Theme, SelectedTag, Result); })];
	Actions->AddSlot().FillWidth(1)[Action(
		TEXT("Paste"), [this] { return FWidgetThemeEditorService::CanPasteProperties(Theme, SelectedTag); },
		[this](FString& Result) { return FWidgetThemeEditorService::PasteProperties(Theme, SelectedTag, Result); })];
	Actions->AddSlot().FillWidth(1).Padding(4, 0, 0, 0)[SNew(SButton)
															.HAlign(HAlign_Center)
															.Text(LOCTEXT("Rename", "Rename"))
															.IsEnabled_Lambda([this] { return CanEditSelection() && !WidgetStyleTags::IsRoot(SelectedTag); })
															.OnClicked(this, &FWidgetThemeToolkit::OpenRenameDialog)];
	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	Body->AddSlot().AutoHeight().Padding(4)[Actions];
	Body->AddSlot().AutoHeight().Padding(4)[Types];
	Body->AddSlot().AutoHeight().Padding(4)[SNew(SSearchBox)
												.OnTextChanged_Lambda(
													[this](const FText& Text)
													{
														Search = Text.ToString();
														Refresh();
													})];
	Body->AddSlot().FillHeight(
		1)[SAssignNew(Tree, STreeView<TSharedPtr<FThemeTreeItem>>)
			   .TreeItemsSource(&Roots)
			   .OnGenerateRow_Lambda(
				   [](TSharedPtr<FThemeTreeItem> Item, const TSharedRef<STableViewBase>& Owner)
				   {
					   return SNew(STableRow<TSharedPtr<FThemeTreeItem>>, Owner)
						   .ToolTipText(FText::FromString(
							   Item->Tooltip))[SNew(SHorizontalBox) +
											   SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(FText::FromName(Item->Tag.GetTagName()))] +
											   SHorizontalBox::Slot().AutoWidth().Padding(8, 0)[SNew(STextBlock)
																									.Text(FText::FromString(Item->bRoot		? TEXT("Root")
																															: Item->bShared ? TEXT("Shared")
																																			: TEXT("")))]];
				   })
			   .OnGetChildren_Lambda([](TSharedPtr<FThemeTreeItem> Item, TArray<TSharedPtr<FThemeTreeItem>>& Children) { Children = Item->Children; })
			   .OnSelectionChanged(this, &FWidgetThemeToolkit::Select)];
	Refresh();
	return SNew(SDockTab).ShouldAutosize(false)[Body];
}

TSharedRef<SDockTab> FWidgetThemeToolkit::SpawnPreview(const FSpawnTabArgs& Args)
{
	auto Visibility = [this](EWidgetStyleKind Type)
	{
		EWidgetStyleKind SelectedType;
		return FWidgetThemeEditorService::Kind(SelectedTag, SelectedType) && (Type == EWidgetStyleKind::None || SelectedType == Type) &&
					   !WidgetStyleTags::IsRoot(SelectedTag)
				   ? EVisibility::Visible
				   : EVisibility::Collapsed;
	};
	auto Dimension = [this](bool Width) -> TSharedRef<SWidget>
	{
		return SNew(SBox).WidthOverride(64)[SNew(SNumericEntryBox<float>)
												.MinValue(1)
												.MinSliderValue(1)
												.MaxSliderValue(Width ? 500 : 350)
												.AllowSpin(true)
												.Value_Lambda([this, Width]() -> TOptional<float> { return Width ? PreviewSize.X : PreviewSize.Y; })
												.OnValueChanged_Lambda(
													[this, Width](float Value)
													{
														if (Width)
														{
															PreviewSize.X = Value;
															bPreviewWidthEdited = true;
														}
														else
														{
															PreviewSize.Y = Value;
															bPreviewHeightEdited = true;
														}
													})];
	};
	TSharedRef<SHorizontalBox> Header = SNew(SHorizontalBox);
	Header->AddSlot()
		.AutoWidth()
		.Padding(0, 0, 8, 0)
		.VAlign(VAlign_Center)[SNew(SCheckBox)
								   .Visibility_Lambda([Visibility] { return Visibility(EWidgetStyleKind::Button); })
								   .IsChecked_Lambda([this] { return PreviewButton->GetSelected() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
								   .OnCheckStateChanged_Lambda(
									   [this](ECheckBoxState State)
									   {
										   PreviewButton->SetIsSelected(State == ECheckBoxState::Checked, false);
									   })[SNew(STextBlock).Text(LOCTEXT("Selected", "Selected"))]];
	Header->AddSlot()
		.AutoWidth()
		.Padding(0, 0, 8, 0)
		.VAlign(VAlign_Center)[SNew(SCheckBox)
								   .Visibility_Lambda([Visibility] { return Visibility(EWidgetStyleKind::None); })
								   .IsChecked_Lambda([this] { return PreviewButton->GetIsEnabled() ? ECheckBoxState::Unchecked : ECheckBoxState::Checked; })
								   .OnCheckStateChanged_Lambda(
									   [this](ECheckBoxState State)
									   {
										   const bool Enabled = State != ECheckBoxState::Checked;
										   PreviewButton->SetIsEnabled(Enabled);
										   PreviewText->SetIsEnabled(Enabled);
										   PreviewImage->SetIsEnabled(Enabled);
									   })[SNew(STextBlock).Text(LOCTEXT("Disabled", "Disabled"))]];
	Header->AddSlot()
		.AutoWidth()
		.Padding(0, 0, 8, 0)
		.VAlign(VAlign_Center)[SNew(SCheckBox)
								   .Visibility_Lambda([Visibility] { return Visibility(EWidgetStyleKind::Button); })
								   .IsChecked_Lambda([this] { return bShowPreviewText ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
								   .OnCheckStateChanged_Lambda(
									   [this](ECheckBoxState State)
									   {
										   bShowPreviewText = State == ECheckBoxState::Checked;
										   PreviewButton->SetPreviewTextVisible(bShowPreviewText);
									   })[SNew(STextBlock).Text(LOCTEXT("ShowText", "Show text"))]];
	Header->AddSlot().FillWidth(1)[SNullWidget::NullWidget];
	Header->AddSlot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(LOCTEXT("Width", "W"))];
	Header->AddSlot().AutoWidth().Padding(4, 0)[Dimension(true)];
	Header->AddSlot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(LOCTEXT("Height", "H"))];
	Header->AddSlot().AutoWidth().Padding(4, 0)[Dimension(false)];
	auto PreviewBox = [this, Visibility](EWidgetStyleKind Type, TSharedRef<SWidget> Widget) -> TSharedRef<SWidget>
	{
		return SNew(SBox)
			.HAlign(Type == EWidgetStyleKind::Text ? HAlign_Center : HAlign_Fill)
			.VAlign(Type == EWidgetStyleKind::Text ? VAlign_Center : VAlign_Fill)
			.WidthOverride_Lambda([this] { return PreviewSize.X; })
			.HeightOverride_Lambda([this] { return PreviewSize.Y; })
			.Visibility_Lambda([Visibility, Type] { return Visibility(Type); })[Widget];
	};
	return SNew(SDockTab).ShouldAutosize(
		false)[SNew(SOverlay) +
			   SOverlay::Slot()
				   .HAlign(HAlign_Center)
				   .VAlign(VAlign_Center)[SNew(SVerticalBox) +
										  SVerticalBox::Slot().AutoHeight()[PreviewBox(EWidgetStyleKind::Button, PreviewButton->TakeWidget())] +
										  SVerticalBox::Slot().AutoHeight()[PreviewBox(EWidgetStyleKind::Text, PreviewText->TakeWidget())] +
										  SVerticalBox::Slot().AutoHeight()[PreviewBox(EWidgetStyleKind::Brush, PreviewImage->TakeWidget())]] +
			   SOverlay::Slot().VAlign(VAlign_Top).Padding(8)[Header]];
}

TSharedRef<SDockTab> FWidgetThemeToolkit::SpawnDetails(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab).ShouldAutosize(false)[Details.ToSharedRef()];
}

TSharedRef<SDockTab> FWidgetThemeToolkit::SpawnContext(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab).ShouldAutosize(
		false)[SNew(SScrollBox).ScrollBarAlwaysVisible(true) +
			   SScrollBox::Slot().Padding(8)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return FText::FromString(Report); })]];
}

void FWidgetThemeToolkit::Refresh()
{
	Roots = FWidgetThemeTree::Build(Theme, TypeFilter, Search);
	if (Tree)
	{
		Tree->RequestTreeRefresh();
		TFunction<void(TSharedPtr<FThemeTreeItem>)> Expand = [&](TSharedPtr<FThemeTreeItem> Item)
		{
			Tree->SetItemExpansion(Item, true);
			if (Item->Tag == SelectedTag)
			{
				Tree->SetSelection(Item);
			}
			for (const auto& Child : Item->Children)
			{
				Expand(Child);
			}
		};
		for (const auto& Root : Roots)
		{
			Expand(Root);
		}
	}
	BindDetails();
	UpdatePreview();
}

void FWidgetThemeToolkit::Select(TSharedPtr<FThemeTreeItem> Item, ESelectInfo::Type Type)
{
	FinishDetailsEdit();
	const FGameplayTag NewSelection = Item ? Item->Tag : FGameplayTag();
	if (SelectedTag != NewSelection)
	{
		bPreviewWidthEdited = false;
		bPreviewHeightEdited = false;
	}
	SelectedTag = NewSelection;
	FString RemoveReason;
	bCanRemoveSelection = FWidgetThemeEditorService::CanRemove(Theme, SelectedTag, RemoveReason);
	BindDetails();
	UpdatePreview();
}

void FWidgetThemeToolkit::BindDetails()
{
	EWidgetStyleKind Type;
	UClass* StyleClass = FWidgetThemeEditorService::Kind(SelectedTag, Type) ? Theme->GetStyleClass(SelectedTag, Type) : nullptr;
	Details->SetObject(StyleClass ? StyleClass->GetDefaultObject() : nullptr, true);
}

void FWidgetThemeToolkit::UpdatePreview()
{
	EWidgetStyleKind Type;
	if (!FWidgetThemeEditorService::Kind(SelectedTag, Type) || WidgetStyleTags::IsRoot(SelectedTag))
	{
		return;
	}
	UClass* StyleClass = Theme->GetStyleClass(SelectedTag, Type);
	if (!StyleClass)
	{
		return;
	}
	UpdatePreviewSize(StyleClass->GetDefaultObject(), Type);
	if (Type == EWidgetStyleKind::Button)
	{
		PreviewButton->SetStyleTag(SelectedTag);
		PreviewButton->SetStyle(Theme->GetStyleClass(SelectedTag, Type));
		PreviewButton->SetPreviewTextVisible(bShowPreviewText);
	}
	else if (Type == EWidgetStyleKind::Text)
	{
		PreviewText->SetStyleTag(SelectedTag);
		PreviewText->SetStyle(Theme->GetStyleClass(SelectedTag, Type));
	}
	else
	{
		PreviewImage->SetStyleTag(SelectedTag);
		if (const UWidgetBrushStyle* Style = Theme->GetStyleClass(SelectedTag, Type)->GetDefaultObject<UWidgetBrushStyle>())
		{
			PreviewImage->SetBrush(Style->Brush);
		}
	}
}

void FWidgetThemeToolkit::UpdatePreviewSize(UObject* Style, EWidgetStyleKind Kind)
{
	FVector2D Size = Kind == EWidgetStyleKind::Brush ? FVector2D(80, 80) : FVector2D(200, 60);
	if (const UCommonButtonStyle* ButtonStyle = Cast<UCommonButtonStyle>(Style))
	{
		if (ButtonStyle->MinWidth > 0)
			Size.X = ButtonStyle->MinWidth;
		if (ButtonStyle->MinHeight > 0)
			Size.Y = ButtonStyle->MinHeight;
	}
	if (!bPreviewWidthEdited)
		PreviewSize.X = Size.X;
	if (!bPreviewHeightEdited)
		PreviewSize.Y = Size.Y;
}

void FWidgetThemeToolkit::NotifyPreChange(FProperty* Property)
{
	if (CanEditSelection())
	{
		Theme->Modify();
		UBlueprint* Style = FWidgetThemeEditorService::Blueprint(Theme, SelectedTag);
		Style->Modify();
		Style->GeneratedClass->GetDefaultObject()->Modify();
		bEditingDetails = true;
	}
}

void FWidgetThemeToolkit::NotifyPostChange(const FPropertyChangedEvent& Event, FProperty* Property)
{
	if (!(Event.ChangeType & EPropertyChangeType::Interactive))
	{
		FinishDetailsEdit();
	}
}

void FWidgetThemeToolkit::FinishDetailsEdit()
{
	if (!bEditingDetails || FWidgetThemeEditorService::IsBusy())
	{
		return;
	}
	bEditingDetails = false;
	if (UBlueprint* Style = FWidgetThemeEditorService::Blueprint(Theme, SelectedTag))
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Style);
		FBlueprintEditorUtils::PostEditChangeBlueprintActors(Style);
		FWidgetThemeResolver::NotifyThemeChanged();
		Report += TEXT("Edited ") + SelectedTag.ToString() + TEXT(". Save to persist.\n");
	}
	UpdatePreview();
}

void FWidgetThemeToolkit::PostUndo(bool Success)
{
	if (Success)
	{
		bEditingDetails = false;
		FWidgetThemeResolver::NotifyThemeChanged();
		Refresh();
	}
}

#undef LOCTEXT_NAMESPACE
