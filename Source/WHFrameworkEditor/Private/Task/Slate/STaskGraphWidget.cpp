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
#include "Task/Blueprint/TaskBlueprintFactory.h"
#include "WHFrameworkEditorStatics.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "Widgets/Layout/SBox.h"
#include "ClassViewerModule.h"
#include "SourceCodeNavigation.h"
#include "IDetailsView.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"

#define LOCTEXT_NAMESPACE "TaskGraph"

namespace TaskGraphClipboard
{
	bool Read(FString& Text)
	{
		for (int32 Attempt = 0; Attempt < 3; ++Attempt)
		{
			FPlatformApplicationMisc::ClipboardPaste(Text);
			if (!Text.IsEmpty()) return true;
			FPlatformProcess::Sleep(0.01f);
		}
		return false;
	}

	bool Write(const FString& Text)
	{
		for (int32 Attempt = 0; Attempt < 3; ++Attempt)
		{
			FPlatformApplicationMisc::ClipboardCopy(*Text);
			FString Copied;
			if (Read(Copied) && Copied == Text) return true;
			FPlatformProcess::Sleep(0.01f);
		}
		return false;
	}
}


void STaskGraphWidget::Construct(const FArguments& InArgs)
{
	TaskEditor = InArgs._TaskEditor;
	Graph = NewObject<UTaskAssetGraph>(GetTransientPackage(), NAME_None, RF_Transient);
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
	Commands->MapAction(FGenericCommands::Get().SelectAll, FExecuteAction::CreateLambda([this] { GraphEditor->SelectAllNodes(); }));
	Commands->MapAction(FGenericCommands::Get().Undo, FExecuteAction::CreateLambda([] { GEditor->UndoTransaction(); }), FCanExecuteAction::CreateSP(this, &STaskGraphWidget::CanEdit));
	Commands->MapAction(FGenericCommands::Get().Redo, FExecuteAction::CreateLambda([] { GEditor->RedoTransaction(); }), FCanExecuteAction::CreateSP(this, &STaskGraphWidget::CanEdit));
	SGraphEditor::FGraphEditorEvents Events;
	Events.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &STaskGraphWidget::OnSelectionChanged);
	Events.OnCreateActionMenuAtLocation = SGraphEditor::FOnCreateActionMenuAtLocation::CreateSP(this, &STaskGraphWidget::CreateActionMenu);
	Events.OnCreateNodeOrPinMenu = SGraphEditor::FOnCreateNodeOrPinMenu::CreateSP(this, &STaskGraphWidget::CreateNodeMenu);
	Events.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &STaskGraphWidget::OnNodeDoubleClicked);
	FGraphAppearanceInfo Appearance;
	Appearance.CornerText = LOCTEXT("GraphTitle", "TASK GRAPH");
	Appearance.InstructionText = LOCTEXT("EmptyGraph", "Right-click to create a task");
	ChildSlot[
		SAssignNew(GraphEditor, SGraphEditor).GraphToEdit(Graph).AdditionalCommands(Commands)
		.GraphEvents(Events).Appearance(Appearance).IsEditable(this, &STaskGraphWidget::CanEdit)
	];
	Rebuild();
}
bool STaskGraphWidget::CanEdit() const { return Graph && FTaskGraphOperations::CanEdit(Graph->TaskAsset); }
bool STaskGraphWidget::CanCopy() const { return !GetSelectedTasks().IsEmpty(); }
bool STaskGraphWidget::HasSelection() const { return CanEdit() && CanCopy(); }
bool STaskGraphWidget::CanPaste() const
{
	if (!CanEdit()) return false;
	const double Now = FPlatformTime::Seconds();
	if (Now - LastClipboardCheckTime >= 0.2)
	{
		FString Text;
		TaskGraphClipboard::Read(Text);
		bCanPasteCached = FTaskGraphOperations::CanImportTasks(Text);
		LastClipboardCheckTime = Now;
	}
	return bCanPasteCached;
}
void STaskGraphWidget::AddReferencedObjects(FReferenceCollector& Collector) { Collector.AddReferencedObject(Graph); }

TArray<UTaskBase*> STaskGraphWidget::GetSelectedTasks() const
{
	TArray<UTaskBase*> Tasks;
	if (GraphEditor) for (UObject* Object : GraphEditor->GetSelectedNodes())
		if (const UTaskAssetGraphNode* Node = Cast<UTaskAssetGraphNode>(Object); Node && Node->Task) Tasks.Add(Node->Task);
	return Tasks;
}

void STaskGraphWidget::Rebuild()
{
	if (!Graph || !Graph->TaskAsset) return;

	{
		TGuardValue<bool> Guard(bSelecting, true);
		GraphEditor->ClearSelectionSet();
		Graph->Nodes.Reset();
		if (Graph->TaskAsset->RebuildTaskMap(false))
		{
			TMap<UTaskBase*, UTaskAssetGraphNode*> Nodes;
			int32 Row = 0;
			TFunction<void(UTaskBase*)> Add = [&](UTaskBase* Task)
			{
				if (CanEdit()) Task->SetFlags(RF_Transactional);
				UTaskAssetGraphNode* Node = NewObject<UTaskAssetGraphNode>(Graph, NAME_None, RF_Transactional);
				Node->Task = Task;
				Node->CreateNewGuid();
				Node->AllocateDefaultPins();
				const FVector2D Position = Task->bHasGraphPosition ? Task->GraphPosition : FVector2D(Task->TaskHierarchy * 320.f, Row * 150.f);
				++Row;
				Node->NodePosX = FMath::RoundToInt(Position.X);
				Node->NodePosY = FMath::RoundToInt(Position.Y);
				Graph->AddNode(Node, false, false);
				Nodes.Add(Task, Node);
				for (UTaskBase* Child : Task->SubTasks) Add(Child);
			};
			for (UTaskBase* Task : Graph->TaskAsset->RootTasks) Add(Task);
			for (const auto& Pair : Nodes)
			{
				for (UTaskBase* Child : Pair.Key->SubTasks)
					Pair.Value->FindPinChecked(TEXT("Out"))->MakeLinkTo(Nodes[Child]->FindPinChecked(TEXT("In")));
				if (SelectedTaskIDs.Contains(Pair.Key->TaskGUID)) GraphEditor->SetNodeSelection(Pair.Value, true);
			}
		}
		GraphEditor->NotifyGraphChanged();
	}
	TaskEditor.Pin()->RefreshDetails();
}
void STaskGraphWidget::OnTasksChanged() { Rebuild(); }
void STaskGraphWidget::OnSelectionChanged(const FGraphPanelSelectionSet& Selection)
{
	if (!bSelecting && !GIsTransacting)
	{
		SelectedTaskIDs.Reset();
		for (UTaskBase* Task : GetSelectedTasks()) SelectedTaskIDs.Add(Task->TaskGUID);
		TaskEditor.Pin()->RefreshDetails();
	}
}
void STaskGraphWidget::SelectTasks(const TArray<UTaskBase*>& Tasks)
{
	SelectedTaskIDs.Reset();
	for (UTaskBase* Task : Tasks) SelectedTaskIDs.Add(Task->TaskGUID);
	{
		TGuardValue<bool> Guard(bSelecting, true);
		GraphEditor->ClearSelectionSet();
		for (UEdGraphNode* Node : Graph->Nodes)
			if (Tasks.Contains(CastChecked<UTaskAssetGraphNode>(Node)->Task)) GraphEditor->SetNodeSelection(Node, true);
	}
	TaskEditor.Pin()->RefreshDetails();
}
void STaskGraphWidget::DeleteSelected()
{
	if (FTaskGraphOperations::DeleteTasks(Graph->TaskAsset, GetSelectedTasks())) Rebuild();
}
void STaskGraphWidget::CopySelected()
{
	FString Text;
	if (FTaskGraphOperations::ExportTasks(Graph->TaskAsset, GetSelectedTasks(), Text) && TaskGraphClipboard::Write(Text))
	{
		bCanPasteCached = true;
		LastClipboardCheckTime = FPlatformTime::Seconds();
	}
}
void STaskGraphWidget::CutSelected()
{
	if (!HasSelection()) return;
	FString Text;
	if (FTaskGraphOperations::ExportTasks(Graph->TaskAsset, GetSelectedTasks(), Text) && TaskGraphClipboard::Write(Text))
	{
		bCanPasteCached = true;
		LastClipboardCheckTime = FPlatformTime::Seconds();
		DeleteSelected();
	}
}
void STaskGraphWidget::Paste()
{
	if (!CanEdit()) return;
	FString Text;
	if (!TaskGraphClipboard::Read(Text)) return;
	const TArray<UTaskBase*> Added = FTaskGraphOperations::ImportTasks(Graph->TaskAsset, Text, FVector2D(GraphEditor->GetPasteLocation2f()));
	if (!Added.IsEmpty()) { Rebuild(); SelectTasks(Added); }
}
void STaskGraphWidget::DuplicateSelected()
{
	if (!HasSelection()) return;
	const TArray<UTaskBase*> Selected = GetSelectedTasks();
	FString Text;
	if (!FTaskGraphOperations::ExportTasks(Graph->TaskAsset, Selected, Text)) return;
	FVector2D Position = FVector2D::ZeroVector;
	int32 Count = 0;
	TFunction<void(UTaskBase*)> Accumulate = [&](UTaskBase* Task)
	{
		for (UEdGraphNode* Item : Graph->Nodes)
			if (CastChecked<UTaskAssetGraphNode>(Item)->Task == Task)
			{
				Position += FVector2D(Item->NodePosX, Item->NodePosY);
				++Count;
				break;
			}
		for (UTaskBase* Child : Task->SubTasks) Accumulate(Child);
	};
	for (UTaskBase* Task : Selected)
		if (!Selected.ContainsByPredicate([&](UTaskBase* Other) { return Other != Task && Other->IsParentOf(Task); })) Accumulate(Task);
	const TArray<UTaskBase*> Added = FTaskGraphOperations::ImportTasks(Graph->TaskAsset, Text, Position / FMath::Max(1, Count) + FVector2D(60, 60));
	if (!Added.IsEmpty()) { Rebuild(); SelectTasks(Added); }
}
void STaskGraphWidget::AutoLayout()
{
	FTaskGraphOperations::AutoLayout(Graph->TaskAsset);
	Rebuild();
	GraphEditor->ZoomToFit(false);
}

void STaskGraphWidget::AddCommonMenu(FMenuBuilder& Menu)
{
	Menu.BeginSection("Edit", LOCTEXT("Edit", "Edit"));
	Menu.AddMenuEntry(FGenericCommands::Get().Undo);
	Menu.AddMenuEntry(FGenericCommands::Get().Redo);
	Menu.AddMenuEntry(FGenericCommands::Get().Cut);
	Menu.AddMenuEntry(FGenericCommands::Get().Copy);
	Menu.AddMenuEntry(FGenericCommands::Get().Paste);
	Menu.AddMenuEntry(FGenericCommands::Get().Duplicate);
	Menu.AddMenuEntry(FGenericCommands::Get().Delete);
	Menu.AddMenuEntry(FGenericCommands::Get().SelectAll);
	Menu.EndSection();
	Menu.BeginSection("Graph", LOCTEXT("Graph", "Graph"));
	Menu.AddMenuEntry(LOCTEXT("Layout", "Auto Layout"), LOCTEXT("LayoutTip", "Arrange tasks by hierarchy"), FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &STaskGraphWidget::AutoLayout), FCanExecuteAction::CreateSP(this, &STaskGraphWidget::CanEdit)));
	Menu.AddMenuEntry(LOCTEXT("Frame", "Frame All"), FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this] { GraphEditor->ZoomToFit(false); })));
	Menu.AddMenuEntry(LOCTEXT("AssetDetails", "Asset Details"), FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this] { SelectTasks({}); })));
	Menu.EndSection();
}

FActionMenuContent STaskGraphWidget::CreateActionMenu(UEdGraph* InGraph, const FVector2f& Position, const TArray<UEdGraphPin*>& Pins, bool bAutoExpand, SGraphEditor::FActionMenuClosed OnClosed)
{
	FMenuBuilder Menu(true, Commands);
	TWeakObjectPtr<UTaskBase> Task;
	EEdGraphPinDirection Direction = EGPD_MAX;
	if (!Pins.IsEmpty())
	{
		Task = CastChecked<UTaskAssetGraphNode>(Pins[0]->GetOwningNode())->Task;
		Direction = Pins[0]->Direction;
	}
	Menu.AddSubMenu(LOCTEXT("Create", "Create Task"), LOCTEXT("CreateTip", "Choose a native or Blueprint task class"),
		FNewMenuDelegate::CreateLambda([this, Task, Direction, Position](FMenuBuilder& Sub)
		{
			Sub.AddWidget(CreateClassPicker(false, Task, Direction, FVector2D(Position)), FText::GetEmpty(), true);
		}), FUIAction(FExecuteAction(), FCanExecuteAction::CreateSP(this, &STaskGraphWidget::CanEdit)), NAME_None, EUserInterfaceActionType::Button);
	Menu.AddMenuEntry(LOCTEXT("NewBlueprint", "New Task Blueprint"), FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([] { UEditorStatics::CreateBlueprintAssetWithDialog(UTaskBlueprintFactory::StaticClass()); }),
		FCanExecuteAction::CreateSP(this, &STaskGraphWidget::CanEdit)));
	AddCommonMenu(Menu);
	FActionMenuContent Content(Menu.MakeWidget());
	Content.OnMenuDismissed.AddLambda([OnClosed] { OnClosed.ExecuteIfBound(); });
	return Content;
}

FActionMenuContent STaskGraphWidget::CreateNodeMenu(UEdGraph* InGraph, const UEdGraphNode* Node, const UEdGraphPin* Pin, FMenuBuilder* Builder, bool bReadOnly)
{
	const UTaskAssetGraphNode* TaskNode = Cast<UTaskAssetGraphNode>(Node ? Node : Pin->GetOwningNode());
	TWeakObjectPtr<UTaskBase> Task = TaskNode->Task;
	if (!GraphEditor->GetSelectedNodes().Contains(const_cast<UTaskAssetGraphNode*>(TaskNode))) SelectTasks({Task.Get()});
	if (Pin)
	{
		const EEdGraphPinDirection Direction = Pin->Direction;
		Builder->AddMenuEntry(FText::Format(LOCTEXT("DisconnectPin", "Disconnect {0}"), FText::FromName(Pin->PinName)), FText::GetEmpty(), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, Task, Direction]
			{
				for (UEdGraphNode* Item : Graph->Nodes)
					if (CastChecked<UTaskAssetGraphNode>(Item)->Task == Task.Get())
					{
						Graph->GetSchema()->BreakPinLinks(*Item->FindPinChecked(Direction == EGPD_Input ? TEXT("In") : TEXT("Out")), true);
						break;
					}
			}), FCanExecuteAction::CreateSP(this, &STaskGraphWidget::HasSelection)));
	}
	Builder->BeginSection("Node", LOCTEXT("Node", "Node"));
	Builder->AddSubMenu(LOCTEXT("ChangeType", "Change Task Type"), LOCTEXT("ChangeTypeTip", "Keep identity, connections and compatible properties"),
		FNewMenuDelegate::CreateLambda([this, Task](FMenuBuilder& Sub)
		{
			Sub.AddWidget(CreateClassPicker(true, Task, EGPD_MAX, FVector2D::ZeroVector), FText::GetEmpty(), true);
		}), FUIAction(FExecuteAction(), FCanExecuteAction::CreateSP(this, &STaskGraphWidget::CanEdit)), NAME_None, EUserInterfaceActionType::Button);
	Builder->AddMenuEntry(Task->GetClass()->ClassGeneratedBy ? LOCTEXT("EditBlueprint", "Edit Task Blueprint") : LOCTEXT("EditCode", "Go to Task Code"),
		FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([this, Task] { if (Task.IsValid()) EditTaskSource(Task.Get()); })));
	Builder->AddMenuEntry(LOCTEXT("DisconnectNode", "Disconnect Node"), LOCTEXT("DisconnectNodeTip", "Disconnect In and Out without deleting tasks"), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this, Task]
		{
			if (!CanEdit() || !Task.IsValid()) return;
			UTaskAssetGraphNode* GraphNode = nullptr;
			for (UEdGraphNode* Item : Graph->Nodes) if (CastChecked<UTaskAssetGraphNode>(Item)->Task == Task.Get()) GraphNode = CastChecked<UTaskAssetGraphNode>(Item);
			if (GraphNode) Graph->GetSchema()->BreakNodeLinks(*GraphNode);
		}), FCanExecuteAction::CreateSP(this, &STaskGraphWidget::CanEdit)));
	Builder->EndSection();
	AddCommonMenu(*Builder);
	return FActionMenuContent(Builder->MakeWidget());
}

TSharedRef<SWidget> STaskGraphWidget::CreateClassPicker(bool bChangeType, TWeakObjectPtr<UTaskBase> Task, EEdGraphPinDirection Direction, FVector2D Position)
{
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.bShowUnloadedBlueprints = true;
	Options.bShowNoneOption = false;
	TSharedRef<FTaskClassFilter> Filter = MakeShared<FTaskClassFilter>();
	Filter->TaskEditor = TaskEditor;
	Options.ClassFilters.Add(Filter);
	return SNew(SBox).WidthOverride(320).HeightOverride(420)[
		FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options,
			FOnClassPicked::CreateSP(this, &STaskGraphWidget::OnClassPicked, bChangeType, Task, Direction, Position))
	];
}
void STaskGraphWidget::OnClassPicked(UClass* Class, bool bChangeType, TWeakObjectPtr<UTaskBase> Task, EEdGraphPinDirection Direction, FVector2D Position)
{
	FSlateApplication::Get().DismissAllMenus();
	UTaskBase* Selected = bChangeType ? FTaskGraphOperations::ChangeTaskType(Graph->TaskAsset, Task.Get(), Class) :
		FTaskGraphOperations::AddTask(Graph->TaskAsset, Class, Position, Direction == EGPD_Output ? Task.Get() : nullptr, Direction == EGPD_Input ? Task.Get() : nullptr);
	if (Selected) { Rebuild(); SelectTasks({Selected}); }
}
void STaskGraphWidget::EditTaskSource(UTaskBase* Task)
{
	if (Task->GetClass()->ClassGeneratedBy) GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Task->GetClass()->ClassGeneratedBy);
	else FSourceCodeNavigation::NavigateToClass(Task->GetClass());
}
void STaskGraphWidget::OnNodeDoubleClicked(UEdGraphNode* Node)
{
	if (const UTaskAssetGraphNode* TaskNode = Cast<UTaskAssetGraphNode>(Node); TaskNode && TaskNode->Task) EditTaskSource(TaskNode->Task);
}

#undef LOCTEXT_NAMESPACE
