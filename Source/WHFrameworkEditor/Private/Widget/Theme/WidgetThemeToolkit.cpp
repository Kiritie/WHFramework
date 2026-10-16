#include "Widget/Theme/WidgetThemeToolkit.h"
#include "Widget/Theme/WidgetThemeEditorService.h"
#include "Widget/Theme/WidgetThemePreviewButton.h"
#include "Widget/Theme/WidgetTheme.h"
#include "Widget/Theme/WidgetThemeSettings.h"
#include "Widget/Theme/WidgetThemeResolver.h"
#include "Widget/Common/CommonTextBlockN.h"
#include "Widget/Common/CommonImageN.h"
#include "Widget/Common/CommonInput.h"
#include "Widget/Common/CommonToggle.h"
#include "Widget/Common/CommonRange.h"
#include "Widget/Common/CommonProgress.h"
#include "Widget/Common/CommonScroll.h"
#include "Widget/Common/CommonBorderN.h"

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
			Section.AddMenuEntry("ShowExternalAssetFolders", LOCTEXT("ShowExternalAssetFolders", "Show external asset folders in Content Browser"), FText(), FSlateIcon(),
								 FUIAction(FExecuteAction::CreateLambda(
											   []
											   {
												   auto* Settings = GetMutableDefault<UWidgetThemeEditorSettings>();
												   Settings->bHideExternalAssetFolders = !Settings->bHideExternalAssetFolders;
												   Settings->SaveConfig();
												   UWidgetThemeEditorSettings::ApplyFolderVisibility();
											   }),
										   FCanExecuteAction(),
										   FIsActionChecked::CreateLambda([] { return !GetDefault<UWidgetThemeEditorSettings>()->bHideExternalAssetFolders; })),
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
	auto PrepareControl = [this](UWidget* Control)
	{
		Control->SetDesignerFlags(EWidgetDesignFlags::Designing);
		if (UUserWidget* UserWidget = Cast<UUserWidget>(Control))
		{
			UserWidget->Initialize();
		}
	};
	UCommonInput* Input = NewObject<UCommonInput>(PreviewRoot);
	PrepareControl(Input);
	Input->SetText(LOCTEXT("InputPreview", "Input text 输入"));
	PreviewControls.Add(EWidgetStyleKind::Input, Input);
	UCommonToggle* Toggle = NewObject<UCommonToggle>(PreviewRoot);
	PrepareControl(Toggle);
	PreviewControls.Add(EWidgetStyleKind::Toggle, Toggle);
	UCommonRange* Range = NewObject<UCommonRange>(PreviewRoot);
	PrepareControl(Range);
	FCommonRangeConfig RangeConfig;
	RangeConfig.bHasMin = true;
	RangeConfig.bHasMax = true;
	Range->Configure(RangeConfig);
	Range->SetValue(0.5);
	PreviewControls.Add(EWidgetStyleKind::Range, Range);
	UCommonProgress* Progress = NewObject<UCommonProgress>(PreviewRoot);
	PrepareControl(Progress);
	Progress->SetPercent(0.5f);
	PreviewControls.Add(EWidgetStyleKind::Progress, Progress);
	UCommonScroll* Scroll = NewObject<UCommonScroll>(PreviewRoot);
	PrepareControl(Scroll);
	for (int32 Index = 0; Index < 5; ++Index)
	{
		UCommonTextBlockN* Label = NewObject<UCommonTextBlockN>(Scroll);
		Label->SetText(FText::Format(LOCTEXT("ScrollPreview", "Scroll item {0}"), FText::AsNumber(Index + 1)));
		Scroll->AddChild(Label);
	}
	PreviewControls.Add(EWidgetStyleKind::Scroll, Scroll);
	UCommonBorderN* Border = NewObject<UCommonBorderN>(PreviewRoot);
	PrepareControl(Border);
	UCommonTextBlockN* BorderText = NewObject<UCommonTextBlockN>(Border);
	BorderText->SetText(LOCTEXT("BorderPreview", "Border 面板"));
	Border->SetContent(BorderText);
	PreviewControls.Add(EWidgetStyleKind::Border, Border);
	UCommonTextBlockN* TextScroll = NewObject<UCommonTextBlockN>(PreviewRoot);
	PrepareControl(TextScroll);
	TextScroll->SetText(LOCTEXT("TextScrollPreview", "Long text scroll preview — 长文字滚动效果预览 Aa 0123456789"));
	TextScroll->SetScrollingEnabled(true);
	PreviewControls.Add(EWidgetStyleKind::TextScroll, TextScroll);
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
				RequestRefresh();
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
			else if (Object)
			{
				for (EWidgetStyleKind Kind : WidgetStyleTags::Kinds())
				{
					if (Object->IsA(FWidgetThemeResolver::NativeBase(Kind)))
					{
						UpdatePreview();
						break;
					}
				}
			}
		});
	CompileHandle = GEditor->OnBlueprintCompiled().AddLambda(
		[this]
		{
			if (!FWidgetThemeEditorService::IsBusy() && !bEditingDetails)
			{
				RequestRefresh();
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
	FTSTicker::GetCoreTicker().RemoveTicker(RefreshHandle);
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
	for (auto& Control : PreviewControls)
	{
		Collector.AddReferencedObject(Control.Value);
	}
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
	StyleTypes.Reset();
	StyleTypes.Add(MakeShared<EWidgetStyleKind>(EWidgetStyleKind::None));
	for (EWidgetStyleKind Type : WidgetStyleTags::Kinds())
	{
		StyleTypes.Add(MakeShared<EWidgetStyleKind>(Type));
	}
	TSharedRef<SWidget> Types = SNew(SComboBox<TSharedPtr<EWidgetStyleKind>>)
		.OptionsSource(&StyleTypes)
		.InitiallySelectedItem(StyleTypes[0])
		.OnGenerateWidget_Lambda([](TSharedPtr<EWidgetStyleKind> Type)
		{
			return SNew(STextBlock).Text(FText::FromString(WidgetStyleTags::Label(*Type)));
		})
		.OnSelectionChanged_Lambda([this](TSharedPtr<EWidgetStyleKind> Type, ESelectInfo::Type)
		{
			if (Type)
			{
				TypeFilter = *Type;
				Refresh();
			}
		})[SNew(STextBlock).Text_Lambda([this] { return FText::FromString(WidgetStyleTags::Label(TypeFilter)); })];
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
										   for (const auto& Control : PreviewControls)
										   {
											   Control.Value->SetIsEnabled(Enabled);
										   }
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
	TSharedRef<SVerticalBox> Previews = SNew(SVerticalBox);
	Previews->AddSlot().AutoHeight()[PreviewBox(EWidgetStyleKind::Button, PreviewButton->TakeWidget())];
	Previews->AddSlot().AutoHeight()[PreviewBox(EWidgetStyleKind::Text, PreviewText->TakeWidget())];
	Previews->AddSlot().AutoHeight()[PreviewBox(EWidgetStyleKind::Brush, PreviewImage->TakeWidget())];
	for (const auto& Control : PreviewControls)
	{
		Previews->AddSlot().AutoHeight()[PreviewBox(Control.Key, Control.Value->TakeWidget())];
	}
	return SNew(SDockTab).ShouldAutosize(false)[SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[Previews]
		+ SOverlay::Slot().VAlign(VAlign_Top).Padding(8)[Header]];

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

void FWidgetThemeToolkit::RequestRefresh()
{
	if (!RefreshHandle.IsValid())
	{
		RefreshHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &FWidgetThemeToolkit::HandleDeferredRefresh));
	}
}

bool FWidgetThemeToolkit::HandleDeferredRefresh(float DeltaTime)
{
	if (FWidgetThemeEditorService::IsBusy() || bEditingDetails)
	{
		return true;
	}
	RefreshHandle.Reset();
	Refresh();
	return false;
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
	UClass* StyleClass = FWidgetThemeEditorService::Kind(SelectedTag, Type) ? Theme->ResolveStyleClass(SelectedTag, Type) : nullptr;
	Details->SetObject(StyleClass ? StyleClass->GetDefaultObject() : nullptr, true);
}

void FWidgetThemeToolkit::UpdatePreview()
{
	EWidgetStyleKind Type;
	if (!FWidgetThemeEditorService::Kind(SelectedTag, Type) || WidgetStyleTags::IsRoot(SelectedTag))
	{
		return;
	}
	UClass* StyleClass = Theme->ResolveStyleClass(SelectedTag, Type);
	if (!StyleClass)
	{
		return;
	}
	UpdatePreviewSize(StyleClass->GetDefaultObject(), Type);
	if (Type == EWidgetStyleKind::Button)
	{
		PreviewButton->SetStyleTag(SelectedTag);
		PreviewButton->SetStyle(Theme->ResolveStyleClass(SelectedTag, Type));
		PreviewButton->SetPreviewTextVisible(bShowPreviewText);
	}
	else if (Type == EWidgetStyleKind::Text)
	{
		PreviewText->SetStyleTag(SelectedTag);
		PreviewText->SetStyle(Theme->ResolveStyleClass(SelectedTag, Type));
	}
	else if (Type == EWidgetStyleKind::Brush)
	{
		PreviewImage->SetStyleTag(SelectedTag);
		PreviewImage->SetBrush(StyleClass->GetDefaultObject<UWidgetBrushStyle>()->Brush);
	}
	else if (TObjectPtr<UWidget>* Control = PreviewControls.Find(Type))
	{
		if (UCommonWidget* Common = Cast<UCommonWidget>(Control->Get()))
		{
			Common->SetStyleTag(SelectedTag);
		}
		else if (UCommonScroll* Scroll = Cast<UCommonScroll>(Control->Get()))
		{
			Scroll->SetStyleTag(SelectedTag);
		}
		else if (UCommonBorderN* Border = Cast<UCommonBorderN>(Control->Get()))
		{
			Border->SetStyleTag(SelectedTag);
		}
		else if (UCommonTextBlockN* TextScroll = Cast<UCommonTextBlockN>(Control->Get()))
		{
			TextScroll->SetTextScrollStyleTag(SelectedTag);
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
