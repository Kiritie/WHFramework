#include "Task/Slate/STaskGraphWidget.h"

#include "Task/TaskEditor.h"
#include "Task/TaskEditorTypes.h"
#include "Task/Graph/TaskAssetGraph.h"
#include "Task/Graph/TaskAssetGraphNode.h"
#include "Task/Graph/TaskAssetGraphSchema.h"
#include "Task/Graph/TaskGraphOperations.h"
#include "Task/Slate/STaskDetailsWidget.h"
#include "Task/Base/TaskAsset.h"
#include "Task/Base/TaskBase.h"
#include "Task/Base/TaskAssetReferenceTask.h"
#include "Task/Blueprint/TaskBlueprintFactory.h"
#include "Common/Graph/AssetGraphMenu.h"
#include "WHFrameworkEditorStatics.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "ClassViewerModule.h"
#include "SourceCodeNavigation.h"
#include "IDetailsView.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Subsystems/AssetEditorSubsystem.h"

#define LOCTEXT_NAMESPACE "TaskGraph"

namespace TaskGraphClipboard
{
bool Read(FString &Text)
{
	for(int32 Attempt = 0; Attempt < 3; ++Attempt)
	{
		FPlatformApplicationMisc::ClipboardPaste(Text);
		if(!Text.IsEmpty())
		{
			return true;
		}
		FPlatformProcess::Sleep(0.01f);
	}
	return false;
}

bool Write(const FString &Text)
{
	for(int32 Attempt = 0; Attempt < 3; ++Attempt)
	{
		FPlatformApplicationMisc::ClipboardCopy(*Text);
		FString Copied;
		if(Read(Copied) && Copied == Text)
		{
			return true;
		}
		FPlatformProcess::Sleep(0.01f);
	}
	return false;
}
} // namespace TaskGraphClipboard

void STaskGraphWidget::Construct(const FArguments &InArgs)
{
	TaskEditor = InArgs._TaskEditor;
	Graph = NewObject<UTaskAssetGraph>(GetTransientPackage(), NAME_None, RF_Transient | RF_Transactional);
	Graph->Schema = UTaskAssetGraphSchema::StaticClass();
	Graph->TaskAsset = TaskEditor.Pin()->GetEditingAsset<UTaskAsset>();
	Graph->OnTasksChanged.BindSP(this, &STaskGraphWidget::OnTasksChanged);
	Commands = MakeShared<FUICommandList>();
	const FCanExecuteAction EditSelection = FCanExecuteAction::CreateSP(this, &STaskGraphWidget::HasSelection);
	Commands->MapAction(FGenericCommands::Get().Delete, FExecuteAction::CreateSP(this, &STaskGraphWidget::DeleteSelected), EditSelection);
	Commands->MapAction(FGenericCommands::Get().Duplicate, FExecuteAction::CreateSP(this, &STaskGraphWidget::DuplicateSelected), EditSelection);
	Commands->MapAction(FGenericCommands::Get().Copy, FExecuteAction::CreateSP(this, &STaskGraphWidget::CopySelected), FCanExecuteAction::CreateSP(this, &STaskGraphWidget::CanCopy));
	Commands->MapAction(FGenericCommands::Get().Cut, FExecuteAction::CreateSP(this, &STaskGraphWidget::CutSelected), EditSelection);
	Commands->MapAction(FGenericCommands::Get().Paste, FExecuteAction::CreateSP(this, &STaskGraphWidget::Paste), FCanExecuteAction::CreateSP(this, &STaskGraphWidget::CanPaste));
	Commands->MapAction(FGenericCommands::Get().SelectAll, FExecuteAction::CreateLambda([this]
																						{ GraphEditor->SelectAllNodes(); }));
	Commands->MapAction(FGenericCommands::Get().Undo, FExecuteAction::CreateLambda([]
																				   { GEditor->UndoTransaction(); }),
						FCanExecuteAction::CreateSP(this, &STaskGraphWidget::CanEdit));
	Commands->MapAction(FGenericCommands::Get().Redo, FExecuteAction::CreateLambda([]
																				   { GEditor->RedoTransaction(); }),
						FCanExecuteAction::CreateSP(this, &STaskGraphWidget::CanEdit));
	SGraphEditor::FGraphEditorEvents Events;
	Events.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &STaskGraphWidget::OnSelectionChanged);
	Events.OnCreateActionMenuAtLocation = SGraphEditor::FOnCreateActionMenuAtLocation::CreateSP(this, &STaskGraphWidget::CreateActionMenu);
	Events.OnCreateNodeOrPinMenu = SGraphEditor::FOnCreateNodeOrPinMenu::CreateSP(this, &STaskGraphWidget::CreateNodeMenu);
	Events.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &STaskGraphWidget::OnNodeDoubleClicked);
	FGraphAppearanceInfo Appearance;
	Appearance.CornerText = LOCTEXT("GraphTitle", "TASK GRAPH");
	Appearance.InstructionText = LOCTEXT("EmptyGraph", "Right-click to create a task");
	ChildSlot[SAssignNew(GraphEditor, SGraphEditor).GraphToEdit(Graph).AdditionalCommands(Commands).GraphEvents(Events).Appearance(Appearance).IsEditable(this, &STaskGraphWidget::CanEdit)];
	Rebuild();
}
bool STaskGraphWidget::CanEdit() const
{
	return Graph && FTaskGraphOperations::CanEdit(Graph->TaskAsset);
}
bool STaskGraphWidget::CanCopy() const
{
	return !GetSelectedTasks().IsEmpty();
}
bool STaskGraphWidget::HasSelection() const
{
	return CanEdit() && CanCopy();
}
bool STaskGraphWidget::CanPaste() const
{
	if(!CanEdit())
	{
		return false;
	}
	const double Now = FPlatformTime::Seconds();
	if(Now - LastClipboardCheckTime >= 0.2)
	{
		FString Text;
		TaskGraphClipboard::Read(Text);
		bCanPasteCached = FTaskGraphOperations::CanImportTasks(Text);
		LastClipboardCheckTime = Now;
	}
	return bCanPasteCached;
}
void STaskGraphWidget::AddReferencedObjects(FReferenceCollector &Collector)
{
	Collector.AddReferencedObject(Graph);
}

TArray<UTaskBase *> STaskGraphWidget::GetSelectedTasks() const
{
	TArray<UTaskBase *> Tasks;
	if(GraphEditor)
	{
		for(UObject *Object : GraphEditor->GetSelectedNodes())
		{
			if(const UTaskAssetGraphNode *Node = Cast<UTaskAssetGraphNode>(Object); Node && Node->Task)
			{
				Tasks.Add(Node->Task);
			}
		}
	}
	return Tasks;
}

void STaskGraphWidget::Rebuild()
{
	if(!Graph || !Graph->TaskAsset)
	{
		return;
	}
	{
		TGuardValue<bool> Guard(bSelecting, true);
		GraphEditor->ClearSelectionSet();
		Graph->RebuildFromAsset();
		for(UEdGraphNode *Item : Graph->Nodes)
		{
			UTaskAssetGraphNode *Node = CastChecked<UTaskAssetGraphNode>(Item);
			if(SelectedTaskIDs.Contains(Node->GetStableGUID()))
			{
				GraphEditor->SetNodeSelection(Node, true);
			}
		}
		GraphEditor->NotifyGraphChanged();
	}
	TaskEditor.Pin()->RefreshDetails();
}
void STaskGraphWidget::OnTasksChanged()
{
	if(!bRebuildQueued)
	{
		bRebuildQueued = true;
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &STaskGraphWidget::RebuildLater));
	}
}

EActiveTimerReturnType STaskGraphWidget::RebuildLater(double CurrentTime, float DeltaTime)
{
	bRebuildQueued = false;
	Rebuild();
	return EActiveTimerReturnType::Stop;
}
void STaskGraphWidget::OnSelectionChanged(const FGraphPanelSelectionSet &Selection)
{
	if(!bSelecting && !GIsTransacting)
	{
		SelectedTaskIDs.Reset();
		for(UObject *Object : Selection)
		{
			if(UTaskAssetGraphNode *Node = Cast<UTaskAssetGraphNode>(Object))
			{
				SelectedTaskIDs.Add(Node->GetStableGUID());
			}
		}
		TaskEditor.Pin()->RefreshDetails();
	}
}
void STaskGraphWidget::SelectTasks(const TArray<UTaskBase *> &Tasks)
{
	SelectedTaskIDs.Reset();
	for(UTaskBase *Task : Tasks)
	{
		if(Task)
		{
			SelectedTaskIDs.Add(Task->TaskGUID);
		}
	}
	{
		TGuardValue<bool> Guard(bSelecting, true);
		GraphEditor->ClearSelectionSet();
		for(UEdGraphNode *Node : Graph->Nodes)
		{
			if(Tasks.Contains(CastChecked<UTaskAssetGraphNode>(Node)->Task))
			{
				GraphEditor->SetNodeSelection(Node, true);
			}
		}
	}
	TaskEditor.Pin()->RefreshDetails();
}
void STaskGraphWidget::DeleteSelected()
{
	if(FTaskGraphOperations::DeleteTasks(Graph->TaskAsset, GetSelectedTasks()))
	{
		Rebuild();
	}
}
void STaskGraphWidget::CopySelected()
{
	FString Text;
	if(FTaskGraphOperations::ExportTasks(Graph->TaskAsset, GetSelectedTasks(), Text) && TaskGraphClipboard::Write(Text))
	{
		bCanPasteCached = true;
		LastClipboardCheckTime = FPlatformTime::Seconds();
	}
}
void STaskGraphWidget::CutSelected()
{
	if(!HasSelection())
	{
		return;
	}
	FString Text;
	if(FTaskGraphOperations::ExportTasks(Graph->TaskAsset, GetSelectedTasks(), Text) && TaskGraphClipboard::Write(Text))
	{
		bCanPasteCached = true;
		LastClipboardCheckTime = FPlatformTime::Seconds();
		DeleteSelected();
	}
}
void STaskGraphWidget::Paste()
{
	if(!CanEdit())
	{
		return;
	}
	FString Text;
	if(!TaskGraphClipboard::Read(Text))
	{
		return;
	}
	const TArray<UTaskBase *> Added = FTaskGraphOperations::ImportTasks(Graph->TaskAsset, Text, FVector2D(GraphEditor->GetPasteLocation2f()));
	if(!Added.IsEmpty())
	{
		Rebuild();
		SelectTasks(Added);
	}
}
void STaskGraphWidget::DuplicateSelected()
{
	if(!HasSelection())
	{
		return;
	}
	const TArray<UTaskBase *> Selected = GetSelectedTasks();
	FString Text;
	if(!FTaskGraphOperations::ExportTasks(Graph->TaskAsset, Selected, Text))
	{
		return;
	}
	FVector2D Position = FVector2D::ZeroVector;
	for(UTaskBase *Task : Selected)
	{
		Position += Task->GraphPosition;
	}
	const int32 Count = Selected.Num();
	const TArray<UTaskBase *> Added = FTaskGraphOperations::ImportTasks(Graph->TaskAsset, Text, Position / FMath::Max(1, Count) + FVector2D(60, 60));
	if(!Added.IsEmpty())
	{
		Rebuild();
		SelectTasks(Added);
	}
}
void STaskGraphWidget::AutoLayout()
{
	FTaskGraphOperations::AutoLayout(Graph->TaskAsset);
	Rebuild();
	GraphEditor->ZoomToFit(false);
}

void STaskGraphWidget::AddCommonMenu(FMenuBuilder &Menu)
{
	Menu.BeginSection("Migration", LOCTEXT("Migration", "Migration"));
	Menu.AddMenuEntry(LOCTEXT("Migrate", "Create Start Graph From Existing Tree"), LOCTEXT("MigrateTip", "Explicit one-time conversion; preserves task identities and authored content"), FSlateIcon(),
					  FUIAction(FExecuteAction::CreateLambda([this]
															 {
			TArray<FText> Errors;
			if(FTaskGraphOperations::MigrateAsset(Graph->TaskAsset, Errors))
			{
				Rebuild();
			} }),
								FCanExecuteAction::CreateLambda([this]
																{ return CanEdit() && !Graph->TaskAsset->StartGUID.IsValid(); })));

	Menu.EndSection();
	FAssetGraphMenu::AddCommon(Menu,
		FUIAction(FExecuteAction::CreateSP(this, &STaskGraphWidget::AutoLayout), FCanExecuteAction::CreateSP(this, &STaskGraphWidget::CanEdit)),
		FExecuteAction::CreateLambda([this] { GraphEditor->ZoomToFit(false); }),
		FExecuteAction::CreateLambda([this] { SelectTasks({}); }));
}

FActionMenuContent STaskGraphWidget::CreateActionMenu(UEdGraph *InGraph, const FVector2f &Position, const TArray<UEdGraphPin *> &Pins, bool bAutoExpand, SGraphEditor::FActionMenuClosed OnClosed)
{
	FMenuBuilder Menu(true, Commands);
	TWeakObjectPtr<UTaskBase> Task;
	EEdGraphPinDirection Direction = EGPD_MAX;
	bool bFromStart = false;
	if(!Pins.IsEmpty())
	{
		if(UTaskAssetGraphNode *Node = Cast<UTaskAssetGraphNode>(Pins[0]->GetOwningNode()))
		{
			Task = Node->Task;
			bFromStart = Node->bStartNode;
			Direction = Pins[0]->Direction;
		}
	}
	Menu.AddSubMenu(LOCTEXT("Create", "Create Task Node"), LOCTEXT("CreateTip", "Choose a native or Blueprint task class"),
					FNewMenuDelegate::CreateLambda([this, Task, Direction, Position, bFromStart](FMenuBuilder &Sub)
												   { Sub.AddWidget(CreateClassPicker(false, Task, Direction, FVector2D(Position), bFromStart), FText::GetEmpty(), true); }),
					FUIAction(FExecuteAction(), FCanExecuteAction::CreateSP(this, &STaskGraphWidget::CanEdit)), NAME_None, EUserInterfaceActionType::Button);
	Menu.AddMenuEntry(LOCTEXT("NewBlueprint", "New Task Blueprint"), FText::GetEmpty(), FSlateIcon(),
					  FUIAction(FExecuteAction::CreateLambda([]
															 { UEditorStatics::CreateBlueprintAssetWithDialog(UTaskBlueprintFactory::StaticClass()); }),
								FCanExecuteAction::CreateSP(this, &STaskGraphWidget::CanEdit)));
	AddCommonMenu(Menu);
	FActionMenuContent Content(Menu.MakeWidget());
	Content.OnMenuDismissed.AddLambda([OnClosed]
									  { OnClosed.ExecuteIfBound(); });
	return Content;
}

FActionMenuContent STaskGraphWidget::CreateNodeMenu(UEdGraph *InGraph, const UEdGraphNode *Node, const UEdGraphPin *Pin, FMenuBuilder *Builder, bool bReadOnly)
{
	const UTaskAssetGraphNode *TaskNode = Cast<UTaskAssetGraphNode>(Node ? Node : Pin ? Pin->GetOwningNode()
																					  : nullptr);
	if(!TaskNode || !Builder)
	{
		return FActionMenuContent(SNullWidget::NullWidget);
	}
	const FString StableGUID = TaskNode->GetStableGUID();
	TWeakObjectPtr<UTaskBase> Task = TaskNode->Task;
	if(!GraphEditor->GetSelectedNodes().Contains(const_cast<UTaskAssetGraphNode *>(TaskNode)))
	{
		GraphEditor->ClearSelectionSet();
		GraphEditor->SetNodeSelection(const_cast<UTaskAssetGraphNode *>(TaskNode), true);
	}
	if(Pin)
	{
		const EEdGraphPinDirection Direction = Pin->Direction;
		Builder->AddMenuEntry(FAssetGraphMenu::GetDisconnectPinLabel(), FText::GetEmpty(), FSlateIcon(),
							  FUIAction(FExecuteAction::CreateLambda([this, StableGUID, Direction]
																	 {
				if(UTaskAssetGraphNode* Current = Graph->FindNode(StableGUID))
				{
					if(UEdGraphPin* CurrentPin = Current->FindPin(Direction == EGPD_Input ? TEXT("In") : TEXT("Out")))
					{
						Graph->GetSchema()->BreakPinLinks(*CurrentPin, true);
					}
				} }),
										FCanExecuteAction::CreateSP(this, &STaskGraphWidget::CanEdit)));
	}
	Builder->BeginSection("Node", LOCTEXT("Node", "Node"));
	if(!TaskNode->bStartNode && Task.IsValid())
	{
		FAssetGraphMenu::AddEditing(*Builder,
			FNewMenuDelegate::CreateLambda([this, Task](FMenuBuilder& Sub)
			{
				Sub.AddWidget(CreateClassPicker(true, Task, EGPD_MAX, FVector2D::ZeroVector), FText::GetEmpty(), true);
			}), FCanExecuteAction::CreateSP(this, &STaskGraphWidget::CanEdit),
			FExecuteAction::CreateLambda([this, StableGUID]
			{
				if(UTaskAssetGraphNode* Current = Graph->FindNode(StableGUID); Current && Current->Task)
				{
					EditTaskSource(Current->Task);
				}
			}), Task->IsA<UTaskAssetReferenceTask>());
	}
	Builder->AddMenuEntry(FAssetGraphMenu::GetDisconnectNodeLabel(), LOCTEXT("DisconnectNodeTip", "Keep authored nodes while removing their links"), FSlateIcon(),
						  FUIAction(FExecuteAction::CreateLambda([this, StableGUID]
																 {
			if(CanEdit())
			{
				if(UTaskAssetGraphNode* Current = Graph->FindNode(StableGUID))
				{
					Graph->GetSchema()->BreakNodeLinks(*Current);
				}
			} }),
									FCanExecuteAction::CreateSP(this, &STaskGraphWidget::CanEdit)));
	Builder->EndSection();
	AddCommonMenu(*Builder);
	return FActionMenuContent(Builder->MakeWidget());
}

TSharedRef<SWidget> STaskGraphWidget::CreateClassPicker(bool bChangeType, TWeakObjectPtr<UTaskBase> Task, EEdGraphPinDirection Direction, FVector2D Position, bool bFromStart)
{
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.bShowUnloadedBlueprints = true;
	Options.bShowNoneOption = false;
	TSharedRef<FTaskClassFilter> Filter = MakeShared<FTaskClassFilter>();
	Filter->TaskEditor = TaskEditor;
	Options.ClassFilters.Add(Filter);
	return SNew(SBox).WidthOverride(320).HeightOverride(420)[FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, FOnClassPicked::CreateSP(this, &STaskGraphWidget::OnClassPicked, bChangeType, Task, Direction, Position, bFromStart))];
}
void STaskGraphWidget::OnClassPicked(UClass *Class, bool bChangeType, TWeakObjectPtr<UTaskBase> Task, EEdGraphPinDirection Direction, FVector2D Position, bool bFromStart)
{
	FSlateApplication::Get().DismissAllMenus();
	UTaskBase *Current = Task.IsValid() ? Graph->TaskAsset->FindAuthoredTask(Task->TaskGUID) : nullptr;
	if(bChangeType && !Current)
	{
		return;
	}
	UTaskBase *Selected = bChangeType ? FTaskGraphOperations::ChangeTaskType(Graph->TaskAsset, Current, Class) : FTaskGraphOperations::AddTask(Graph->TaskAsset, Class, Position, Direction == EGPD_Output ? Current : nullptr, Direction == EGPD_Input ? Current : nullptr, bFromStart);
	if(Selected)
	{
		Rebuild();
		SelectTasks({Selected});
	}
}
void STaskGraphWidget::EditTaskSource(UTaskBase *Task)
{
	if(UTaskAssetReferenceTask *ReferenceTask = Cast<UTaskAssetReferenceTask>(Task))
	{
		if(UTaskAsset *ReferencedAsset = ReferenceTask->ReferencedAsset.LoadSynchronous())
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(ReferencedAsset);
		}
		return;
	}
	if(Task->GetClass()->ClassGeneratedBy)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Task->GetClass()->ClassGeneratedBy);
	}
	else
	{
		FSourceCodeNavigation::NavigateToClass(Task->GetClass());
	}
}
void STaskGraphWidget::OnNodeDoubleClicked(UEdGraphNode *Node)
{
	if(const UTaskAssetGraphNode *TaskNode = Cast<UTaskAssetGraphNode>(Node); TaskNode && TaskNode->Task)
	{
		EditTaskSource(TaskNode->Task);
	}
}

#undef LOCTEXT_NAMESPACE
