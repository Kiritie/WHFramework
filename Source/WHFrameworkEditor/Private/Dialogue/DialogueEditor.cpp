#include "Dialogue/DialogueEditor.h"

#include "Dialogue/Asset/DialogueAssetActions.h"
#include "Dialogue/Style/DialogueEditorStyle.h"
#include "Dialogue/Slate/SDialogueViewportWidget.h"
#include "Dialogue/Slate/SDialogueDetailsWidget.h"
#include "AssetToolsModule.h"
#include "Editor.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

#define LOCTEXT_NAMESPACE "FDialogueEditor"

IMPLEMENTATION_EDITOR_MODULE(FDialogueEditorModule)
uint32 FDialogueEditorModule::GameAssetCategory = EAssetTypeCategories::Misc;

FDialogueEditorModule::FDialogueEditorModule()
{
	AppIdentifier = FName("DialogueEditorApp");
}
void FDialogueEditorModule::StartupModule()
{
	FDialogueEditorStyle::Initialize();
	FDialogueEditorCommands::Register();
}
void FDialogueEditorModule::ShutdownModule()
{
	FDialogueEditorCommands::Unregister();
	FDialogueEditorStyle::Shutdown();
}
void FDialogueEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, EAssetTypeCategories::Type AssetCategory, TArray<TSharedPtr<IAssetTypeActions>>& AssetTypeActions)
{
	GameAssetCategory = AssetCategory;
	TSharedRef<IAssetTypeActions> Action = MakeShared<FDialogueAssetActions>(AssetCategory);
	AssetTools.RegisterAssetTypeActions(Action);
	AssetTypeActions.Add(Action);
}
TSharedRef<FDialogueEditor> FDialogueEditorModule::CreateDialogueEditor(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& Host, UDialogueAsset* Asset)
{
	TSharedRef<FDialogueEditor> Editor = MakeShared<FDialogueEditor>();
	Editor->InitAssetEditorBase(Mode, Host, Asset);
	return Editor;
}

FDialogueEditor::FDialogueEditor() : EditingState(MakeShared<FDialogueEditorState>())
{
	ToolkitFName = FName("DialogueEditor");
	BaseToolkitName = LOCTEXT("AppLabel", "Dialogue Editor");
	MenuCategory = BaseToolkitName;
	DefaultLayoutName = FName("DialogueEditor_Layout_v1");
	WorldCentricTabPrefix = TEXT("Dialogue");
	WorldCentricTabColorScale = FLinearColor(0.7f, 0.2f, 0.6f);
}
void FDialogueEditor::InitAssetEditorBase(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& Host, UObject* Asset)
{
	check(Cast<UDialogueAsset>(Asset));
	GEditor->RegisterForUndo(this);
	BindCommands();
	FAssetEditorBase::InitAssetEditorBase(Mode, Host, Asset);
}
FEditorModuleBase* FDialogueEditor::GetEditorModule() const
{
	return &FDialogueEditorModule::Get();
}
TSharedRef<FTabManager::FLayout> FDialogueEditor::CreateDefaultLayout()
{
	return FTabManager::NewLayout(DefaultLayoutName)->AddArea(
		FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
		->Split(FTabManager::NewStack()->SetSizeCoefficient(0.75f)->SetHideTabWell(true)->AddTab("DialogueViewport", ETabState::OpenedTab))
		->Split(FTabManager::NewStack()->SetSizeCoefficient(0.25f)->SetHideTabWell(true)->AddTab("DialogueDetails", ETabState::OpenedTab)));
}
void FDialogueEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorBase::RegisterTabSpawners(InTabManager);
	RegisterTrackedTabSpawner(InTabManager, "DialogueViewport", FOnSpawnTab::CreateSP(this, &FDialogueEditor::SpawnTab_Viewport))
		.SetDisplayName(LOCTEXT("GraphTab", "Dialogue Graph"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));
	RegisterTrackedTabSpawner(InTabManager, "DialogueDetails", FOnSpawnTab::CreateSP(this, &FDialogueEditor::SpawnTab_Details))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}
void FDialogueEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	InTabManager->UnregisterTabSpawner("DialogueViewport");
	InTabManager->UnregisterTabSpawner("DialogueDetails");
	FAssetEditorBase::UnregisterTabSpawners(InTabManager);
}
TSharedRef<SDockTab> FDialogueEditor::SpawnTab_Viewport(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)[SAssignNew(DialogueViewportWidget, SDialogueViewportWidget, SharedThis(this)).Dialogue(GetDialogueBeingEdited())];
}
TSharedRef<SDockTab> FDialogueEditor::SpawnTab_Details(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)[SNew(SDialogueDetailsWidget, SharedThis(this))];
}
void FDialogueEditor::PostUndo(bool bSuccess)
{
	if (!bSuccess || !DialogueViewportWidget) return;
	*EditingState = FDialogueEditorState();
	DialogueViewportWidget->SpawnNodes(INDEX_NONE);
	DialogueViewportWidget->ForceRefresh();
	refreshDetails = true;
	FSlateApplication::Get().DismissAllMenus();
}
void FDialogueEditor::PostRedo(bool bSuccess)
{
	PostUndo(bSuccess);
}
bool FDialogueEditor::ProcessCommandBindings(const FKeyEvent& InKeyEvent) const
{
	return GetToolkitCommands()->ProcessCommandBindings(InKeyEvent) || FAssetEditorBase::ProcessCommandBindings(InKeyEvent);
}
void FDialogueEditor::BindCommands()
{
	const TSharedRef<FUICommandList> Commands = GetToolkitCommands();
	const FCanExecuteAction CanEdit = FCanExecuteAction::CreateLambda([this] { return DialogueViewportWidget.IsValid() && !GEditor->PlayWorld; });
	const FCanExecuteAction HasSelection = FCanExecuteAction::CreateLambda([this] { return DialogueViewportWidget.IsValid() && !GEditor->PlayWorld && !DialogueViewportWidget->SelectedNodes.IsEmpty(); });
	Commands->MapAction(FGenericCommands::Get().Duplicate, FExecuteAction::CreateLambda([this] { DialogueViewportWidget->DuplicateSelected(); }), HasSelection);
	Commands->MapAction(FGenericCommands::Get().Copy, FExecuteAction::CreateLambda([this] { DialogueViewportWidget->CopySelected(); }), HasSelection);
	Commands->MapAction(FGenericCommands::Get().Cut, FExecuteAction::CreateLambda([this] { DialogueViewportWidget->CutSelected(); }), HasSelection);
	Commands->MapAction(FGenericCommands::Get().Paste, FExecuteAction::CreateLambda([this] { DialogueViewportWidget->PasteNodes(); }), CanEdit);
	Commands->MapAction(FGenericCommands::Get().Delete, FExecuteAction::CreateLambda([this] { DialogueViewportWidget->DeleteSelected(); }), HasSelection);
	Commands->MapAction(FDialogueEditorCommands::Get().ValidateDialogue, FExecuteAction::CreateSP(this, &FDialogueEditor::OnValidateDialogue));
}
void FDialogueEditor::ExtendToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.AddToolBarButton(FDialogueEditorCommands::Get().ValidateDialogue);
}
void FDialogueEditor::OnValidateDialogue()
{
	TArray<FText> Errors;
	const bool bValid = GetDialogueBeingEdited()->ValidateDialogue(Errors);
	FNotificationInfo Info(bValid ? LOCTEXT("Valid", "Dialogue is valid") : FText::Join(FText::FromString(TEXT("\n")), Errors));
	Info.ExpireDuration = 5.f;
	FSlateNotificationManager::Get().AddNotification(Info);
}
#undef LOCTEXT_NAMESPACE
