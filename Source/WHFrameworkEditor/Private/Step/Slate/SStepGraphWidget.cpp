#include "Step/Slate/SStepGraphWidget.h"

#include "Step/StepEditor.h"
#include "Step/StepEditorTypes.h"
#include "Step/Graph/StepAssetGraph.h"
#include "Step/Graph/StepAssetGraphNode.h"
#include "Step/Graph/StepAssetGraphSchema.h"
#include "Step/Graph/StepGraphOperations.h"
#include "Step/Slate/SStepDetailsWidget.h"
#include "Step/Base/StepAsset.h"
#include "Step/Base/StepBase.h"
#include "Step/Blueprint/StepBlueprintFactory.h"
#include "Common/Graph/AssetGraphMenu.h"
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

#define LOCTEXT_NAMESPACE "StepGraph"

namespace StepGraphClipboard
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
} // namespace StepGraphClipboard

void SStepGraphWidget::Construct(const FArguments &InArgs)
{
	StepEditor = InArgs._StepEditor;
	Graph = NewObject<UStepAssetGraph>(GetTransientPackage(), NAME_None, RF_Transient | RF_Transactional);
	Graph->Schema = UStepAssetGraphSchema::StaticClass();
	Graph->StepAsset = StepEditor.Pin()->GetEditingAsset<UStepAsset>();
	Graph->OnStepsChanged.BindSP(this, &SStepGraphWidget::OnStepsChanged);
	Commands = MakeShared<FUICommandList>();
	const FCanExecuteAction EditSelection = FCanExecuteAction::CreateSP(this, &SStepGraphWidget::HasSelection);
	Commands->MapAction(FGenericCommands::Get().Delete, FExecuteAction::CreateSP(this, &SStepGraphWidget::DeleteSelected), EditSelection);
	Commands->MapAction(FGenericCommands::Get().Duplicate, FExecuteAction::CreateSP(this, &SStepGraphWidget::DuplicateSelected), EditSelection);
	Commands->MapAction(FGenericCommands::Get().Copy, FExecuteAction::CreateSP(this, &SStepGraphWidget::CopySelected), FCanExecuteAction::CreateSP(this, &SStepGraphWidget::CanCopy));
	Commands->MapAction(FGenericCommands::Get().Cut, FExecuteAction::CreateSP(this, &SStepGraphWidget::CutSelected), EditSelection);
	Commands->MapAction(FGenericCommands::Get().Paste, FExecuteAction::CreateSP(this, &SStepGraphWidget::Paste), FCanExecuteAction::CreateSP(this, &SStepGraphWidget::CanPaste));
	Commands->MapAction(FGenericCommands::Get().SelectAll, FExecuteAction::CreateLambda([this]
																						{ GraphEditor->SelectAllNodes(); }));
	Commands->MapAction(FGenericCommands::Get().Undo, FExecuteAction::CreateLambda([]
																				   { GEditor->UndoTransaction(); }),
						FCanExecuteAction::CreateSP(this, &SStepGraphWidget::CanEdit));
	Commands->MapAction(FGenericCommands::Get().Redo, FExecuteAction::CreateLambda([]
																				   { GEditor->RedoTransaction(); }),
						FCanExecuteAction::CreateSP(this, &SStepGraphWidget::CanEdit));
	SGraphEditor::FGraphEditorEvents Events;
	Events.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &SStepGraphWidget::OnSelectionChanged);
	Events.OnCreateActionMenuAtLocation = SGraphEditor::FOnCreateActionMenuAtLocation::CreateSP(this, &SStepGraphWidget::CreateActionMenu);
	Events.OnCreateNodeOrPinMenu = SGraphEditor::FOnCreateNodeOrPinMenu::CreateSP(this, &SStepGraphWidget::CreateNodeMenu);
	Events.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &SStepGraphWidget::OnNodeDoubleClicked);
	FGraphAppearanceInfo Appearance;
	Appearance.CornerText = LOCTEXT("GraphTitle", "STEP GRAPH");
	Appearance.InstructionText = LOCTEXT("EmptyGraph", "Right-click to create a step");
	ChildSlot[SAssignNew(GraphEditor, SGraphEditor).GraphToEdit(Graph).AdditionalCommands(Commands).GraphEvents(Events).Appearance(Appearance).IsEditable(this, &SStepGraphWidget::CanEdit)];
	Rebuild();
}
bool SStepGraphWidget::CanEdit() const
{
	return Graph && FStepGraphOperations::CanEdit(Graph->StepAsset);
}
bool SStepGraphWidget::CanCopy() const
{
	return !GetSelectedSteps().IsEmpty();
}
bool SStepGraphWidget::HasSelection() const
{
	return CanEdit() && CanCopy();
}
bool SStepGraphWidget::CanPaste() const
{
	if(!CanEdit())
	{
		return false;
	}
	const double Now = FPlatformTime::Seconds();
	if(Now - LastClipboardCheckTime >= 0.2)
	{
		FString Text;
		StepGraphClipboard::Read(Text);
		bCanPasteCached = FStepGraphOperations::CanImportSteps(Text);
		LastClipboardCheckTime = Now;
	}
	return bCanPasteCached;
}
void SStepGraphWidget::AddReferencedObjects(FReferenceCollector &Collector)
{
	Collector.AddReferencedObject(Graph);
}

TArray<UStepBase *> SStepGraphWidget::GetSelectedSteps() const
{
	TArray<UStepBase *> Steps;
	if(GraphEditor)
	{
		for(UObject *Object : GraphEditor->GetSelectedNodes())
		{
			if(const UStepAssetGraphNode *Node = Cast<UStepAssetGraphNode>(Object); Node && Node->Step)
			{
				Steps.Add(Node->Step);
			}
		}
	}
	return Steps;
}

void SStepGraphWidget::Rebuild()
{
	if(!Graph || !Graph->StepAsset)
	{
		return;
	}
	{
		TGuardValue<bool> Guard(bSelecting, true);
		GraphEditor->ClearSelectionSet();
		Graph->Rebuild();
		for(const FString &GUID : SelectedStepIDs)
		{
			if(UStepAssetGraphNode *Node = Graph->FindNode(GUID))
			{
				GraphEditor->SetNodeSelection(Node, true);
			}
		}
		GraphEditor->NotifyGraphChanged();
	}
	StepEditor.Pin()->RefreshDetails();
}
void SStepGraphWidget::OnStepsChanged()
{
	if(!bRebuildQueued)
	{
		bRebuildQueued = true;
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SStepGraphWidget::RebuildLater));
	}
}

EActiveTimerReturnType SStepGraphWidget::RebuildLater(double CurrentTime, float DeltaTime)
{
	bRebuildQueued = false;
	Rebuild();
	return EActiveTimerReturnType::Stop;
}

void SStepGraphWidget::OnSelectionChanged(const FGraphPanelSelectionSet &Selection)
{
	if(!bSelecting && !GIsTransacting)
	{
		SelectedStepIDs.Reset();
		for(UStepBase *Step : GetSelectedSteps())
		{
			SelectedStepIDs.Add(Step->StepGUID);
		}
		StepEditor.Pin()->RefreshDetails();
	}
}
void SStepGraphWidget::SelectSteps(const TArray<UStepBase *> &Steps)
{
	SelectedStepIDs.Reset();
	for(UStepBase *Step : Steps)
	{
		SelectedStepIDs.Add(Step->StepGUID);
	}
	{
		TGuardValue<bool> Guard(bSelecting, true);
		GraphEditor->ClearSelectionSet();
		for(UEdGraphNode *Node : Graph->Nodes)
		{
			if(Steps.Contains(CastChecked<UStepAssetGraphNode>(Node)->Step))
			{
				GraphEditor->SetNodeSelection(Node, true);
			}
		}
	}
	StepEditor.Pin()->RefreshDetails();
}
void SStepGraphWidget::DeleteSelected()
{
	if(FStepGraphOperations::DeleteSteps(Graph->StepAsset, GetSelectedSteps()))
	{
		Rebuild();
	}
}
void SStepGraphWidget::CopySelected()
{
	FString Text;
	if(FStepGraphOperations::ExportSteps(Graph->StepAsset, GetSelectedSteps(), Text) && StepGraphClipboard::Write(Text))
	{
		bCanPasteCached = true;
		LastClipboardCheckTime = FPlatformTime::Seconds();
	}
}
void SStepGraphWidget::CutSelected()
{
	if(!HasSelection())
	{
		return;
	}
	FString Text;
	if(FStepGraphOperations::ExportSteps(Graph->StepAsset, GetSelectedSteps(), Text) && StepGraphClipboard::Write(Text))
	{
		bCanPasteCached = true;
		LastClipboardCheckTime = FPlatformTime::Seconds();
		DeleteSelected();
	}
}
void SStepGraphWidget::Paste()
{
	if(!CanEdit())
	{
		return;
	}
	FString Text;
	if(!StepGraphClipboard::Read(Text))
	{
		return;
	}
	const TArray<UStepBase *> Added = FStepGraphOperations::ImportSteps(Graph->StepAsset, Text, FVector2D(GraphEditor->GetPasteLocation2f()));
	if(!Added.IsEmpty())
	{
		Rebuild();
		SelectSteps(Added);
	}
}
void SStepGraphWidget::DuplicateSelected()
{
	if(!HasSelection())
	{
		return;
	}
	const TArray<UStepBase *> Selected = GetSelectedSteps();
	FString Text;
	if(!FStepGraphOperations::ExportSteps(Graph->StepAsset, Selected, Text))
	{
		return;
	}
	FVector2D Position = FVector2D::ZeroVector;
	for(const UStepBase *Step : Selected)
	{
		Position += Step->GraphPosition;
	}
	const TArray<UStepBase *> Added = FStepGraphOperations::ImportSteps(Graph->StepAsset, Text, Position / Selected.Num() + FVector2D(60, 60));
	if(!Added.IsEmpty())
	{
		Rebuild();
		SelectSteps(Added);
	}
}
void SStepGraphWidget::AutoLayout()
{
	FStepGraphOperations::AutoLayout(Graph->StepAsset);
	Rebuild();
	GraphEditor->ZoomToFit(false);
}

void SStepGraphWidget::AddCommonMenu(FMenuBuilder &Menu)
{
	FAssetGraphMenu::AddCommon(Menu,
		FUIAction(FExecuteAction::CreateSP(this, &SStepGraphWidget::AutoLayout), FCanExecuteAction::CreateSP(this, &SStepGraphWidget::CanEdit)),
		FExecuteAction::CreateLambda([this] { GraphEditor->ZoomToFit(false); }),
		FExecuteAction::CreateLambda([this] { SelectSteps({}); }));
}

FActionMenuContent SStepGraphWidget::CreateActionMenu(UEdGraph *InGraph, const FVector2f &Position, const TArray<UEdGraphPin *> &Pins, bool bAutoExpand, SGraphEditor::FActionMenuClosed OnClosed)
{
	FMenuBuilder Menu(true, Commands);
	FString NodeGUID;
	EEdGraphPinDirection Direction = EGPD_MAX;
	if(!Pins.IsEmpty() && Pins[0])
	{
		if(const UStepAssetGraphNode *Node = Cast<UStepAssetGraphNode>(Pins[0]->GetOwningNode()))
		{
			NodeGUID = Node->StableGUID;
			Direction = Pins[0]->Direction;
		}
	}
	Menu.AddSubMenu(LOCTEXT("Create", "Create Step Node"), LOCTEXT("CreateTip", "Choose a native or Blueprint step class"),
					FNewMenuDelegate::CreateLambda([this, NodeGUID, Direction, Position](FMenuBuilder &Sub)
												   { Sub.AddWidget(CreateClassPicker(false, NodeGUID, Direction, FVector2D(Position)), FText::GetEmpty(), true); }),
					FUIAction(FExecuteAction(), FCanExecuteAction::CreateSP(this, &SStepGraphWidget::CanEdit)), NAME_None, EUserInterfaceActionType::Button);
	Menu.AddMenuEntry(LOCTEXT("NewBlueprint", "New Step Blueprint"), FText::GetEmpty(), FSlateIcon(),
					  FUIAction(FExecuteAction::CreateLambda([]
															 { UEditorStatics::CreateBlueprintAssetWithDialog(UStepBlueprintFactory::StaticClass()); }),
								FCanExecuteAction::CreateSP(this, &SStepGraphWidget::CanEdit)));
	AddCommonMenu(Menu);
	FActionMenuContent Content(Menu.MakeWidget());
	Content.OnMenuDismissed.AddLambda([OnClosed]
									  { OnClosed.ExecuteIfBound(); });
	return Content;
}

FActionMenuContent SStepGraphWidget::CreateNodeMenu(UEdGraph *InGraph, const UEdGraphNode *Node, const UEdGraphPin *Pin, FMenuBuilder *Builder, bool bReadOnly)
{
	const UStepAssetGraphNode *StepNode = Cast<UStepAssetGraphNode>(Node ? Node : (Pin ? Pin->GetOwningNode() : nullptr));
	if(!StepNode || !Builder)
	{
		return FActionMenuContent(SNullWidget::NullWidget);
	}
	const FString GUID = StepNode->StableGUID;
	if(!GraphEditor->GetSelectedNodes().Contains(const_cast<UStepAssetGraphNode *>(StepNode)))
	{
		SelectSteps(StepNode->Step ? TArray<UStepBase *>{StepNode->Step} : TArray<UStepBase *>());
	}
	if(Pin)
	{
		const EEdGraphPinDirection Direction = Pin->Direction;
		Builder->AddMenuEntry(FAssetGraphMenu::GetDisconnectPinLabel(), FText::GetEmpty(), FSlateIcon(),
							  FUIAction(FExecuteAction::CreateLambda([this, GUID, Direction]
																	 {
				UStepAssetGraphNode* CurrentNode = Graph->FindNode(GUID);
				UEdGraphPin* CurrentPin = CurrentNode ? CurrentNode->FindPin(Direction == EGPD_Input ? TEXT("In") : TEXT("Out")) : nullptr;
				if(CurrentPin)
				{
					Graph->GetSchema()->BreakPinLinks(*CurrentPin, true);
				} }),
										FCanExecuteAction::CreateSP(this, &SStepGraphWidget::CanEdit)));
	}
	Builder->BeginSection("Node", LOCTEXT("Node", "Node"));
	if(!StepNode->IsStart())
	{
		FAssetGraphMenu::AddEditing(*Builder,
			FNewMenuDelegate::CreateLambda([this, GUID](FMenuBuilder& Sub)
			{
				Sub.AddWidget(CreateClassPicker(true, GUID, EGPD_MAX, FVector2D::ZeroVector), FText::GetEmpty(), true);
			}), FCanExecuteAction::CreateSP(this, &SStepGraphWidget::CanEdit),
			FExecuteAction::CreateLambda([this, GUID] { EditStepSource(GUID); }));
	}
	Builder->AddMenuEntry(FAssetGraphMenu::GetDisconnectNodeLabel(), LOCTEXT("DisconnectTip", "Disconnect edges and keep all authoring nodes"), FSlateIcon(),
						  FUIAction(FExecuteAction::CreateLambda([this, GUID]
																 {
			if(UStepAssetGraphNode* CurrentNode = Graph->FindNode(GUID))
			{
				Graph->GetSchema()->BreakNodeLinks(*CurrentNode);
			} }),
									FCanExecuteAction::CreateSP(this, &SStepGraphWidget::CanEdit)));
	Builder->EndSection();
	AddCommonMenu(*Builder);
	return FActionMenuContent(Builder->MakeWidget());
}

TSharedRef<SWidget> SStepGraphWidget::CreateClassPicker(bool bChangeType, FString NodeGUID, EEdGraphPinDirection Direction, FVector2D Position)
{
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.bShowUnloadedBlueprints = true;
	Options.bShowNoneOption = false;
	TSharedRef<FStepClassFilter> Filter = MakeShared<FStepClassFilter>();
	Filter->StepEditor = StepEditor;
	Options.ClassFilters.Add(Filter);
	return SNew(SBox).WidthOverride(320).HeightOverride(420)
		[FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, FOnClassPicked::CreateSP(this, &SStepGraphWidget::OnClassPicked, bChangeType, NodeGUID, Direction, Position))];
}

void SStepGraphWidget::OnClassPicked(UClass *Class, bool bChangeType, FString NodeGUID, EEdGraphPinDirection Direction, FVector2D Position)
{
	FSlateApplication::Get().DismissAllMenus();
	UStepBase *Step = Graph->StepAsset->FindDefinition(NodeGUID);
	UStepBase *Selected = bChangeType ? FStepGraphOperations::ChangeStepType(Graph->StepAsset, Step, Class) : FStepGraphOperations::AddStep(Graph->StepAsset, Class, Position, Direction == EGPD_Output ? NodeGUID : FString(), Direction == EGPD_Input ? NodeGUID : FString());
	if(Selected)
	{
		Rebuild();
		SelectSteps({Selected});
	}
}

void SStepGraphWidget::EditStepSource(const FString &NodeGUID)
{
	UStepBase *Step = Graph->StepAsset->FindDefinition(NodeGUID);
	if(!Step)
	{
		return;
	}
	if(Step->GetClass()->ClassGeneratedBy)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Step->GetClass()->ClassGeneratedBy);
	}
	else
	{
		FSourceCodeNavigation::NavigateToClass(Step->GetClass());
	}
}

void SStepGraphWidget::OnNodeDoubleClicked(UEdGraphNode *Node)
{
	if(const UStepAssetGraphNode *StepNode = Cast<UStepAssetGraphNode>(Node))
	{
		EditStepSource(StepNode->StableGUID);
	}
}

#undef LOCTEXT_NAMESPACE
