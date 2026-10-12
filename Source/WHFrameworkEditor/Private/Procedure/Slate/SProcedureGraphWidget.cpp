#include "Procedure/Slate/SProcedureGraphWidget.h"

#include "Procedure/ProcedureEditor.h"
#include "Procedure/ProcedureEditorTypes.h"
#include "Procedure/Base/ProcedureAsset.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Procedure/Graph/ProcedureAssetGraph.h"
#include "Procedure/Graph/ProcedureAssetGraphNode.h"
#include "Procedure/Graph/ProcedureAssetGraphSchema.h"
#include "Procedure/Graph/ProcedureGraphOperations.h"
#include "Procedure/Blueprint/ProcedureBlueprintFactory.h"
#include "Common/Graph/AssetGraphMenu.h"
#include "WHFrameworkEditorStatics.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "ClassViewerModule.h"
#include "SourceCodeNavigation.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"

void SProcedureGraphWidget::Construct(const FArguments& InArgs)
{
    ProcedureEditor = InArgs._ProcedureEditor;
    Graph = NewObject<UProcedureAssetGraph>(GetTransientPackage(), NAME_None, RF_Transient | RF_Transactional);
    Graph->Asset = ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>();
    Graph->Schema = UProcedureAssetGraphSchema::StaticClass();
    Graph->OnChanged.BindSP(this, &SProcedureGraphWidget::OnChanged);
    Commands = MakeShared<FUICommandList>();
    const FCanExecuteAction EditSelection = FCanExecuteAction::CreateSP(this, &SProcedureGraphWidget::HasSelection);
    Commands->MapAction(FGenericCommands::Get().Delete, FExecuteAction::CreateSP(this, &SProcedureGraphWidget::DeleteSelected), EditSelection);
    Commands->MapAction(FGenericCommands::Get().Duplicate, FExecuteAction::CreateSP(this, &SProcedureGraphWidget::DuplicateSelected), EditSelection);
    Commands->MapAction(FGenericCommands::Get().Copy, FExecuteAction::CreateSP(this, &SProcedureGraphWidget::CopySelected), FCanExecuteAction::CreateSP(this, &SProcedureGraphWidget::CanCopy));
    Commands->MapAction(FGenericCommands::Get().Cut, FExecuteAction::CreateSP(this, &SProcedureGraphWidget::CutSelected), EditSelection);
    Commands->MapAction(FGenericCommands::Get().Paste, FExecuteAction::CreateSP(this, &SProcedureGraphWidget::Paste), FCanExecuteAction::CreateSP(this, &SProcedureGraphWidget::CanPaste));
    Commands->MapAction(FGenericCommands::Get().SelectAll, FExecuteAction::CreateLambda([this] { GraphEditor->SelectAllNodes(); }));
    Commands->MapAction(FGenericCommands::Get().Undo, FExecuteAction::CreateLambda([] { GEditor->UndoTransaction(); }), FCanExecuteAction::CreateSP(this, &SProcedureGraphWidget::CanEdit));
    Commands->MapAction(FGenericCommands::Get().Redo, FExecuteAction::CreateLambda([] { GEditor->RedoTransaction(); }), FCanExecuteAction::CreateSP(this, &SProcedureGraphWidget::CanEdit));
    SGraphEditor::FGraphEditorEvents Events;
    Events.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &SProcedureGraphWidget::OnSelectionChanged);
    Events.OnCreateActionMenuAtLocation = SGraphEditor::FOnCreateActionMenuAtLocation::CreateSP(this, &SProcedureGraphWidget::CreateActionMenu);
    Events.OnCreateNodeOrPinMenu = SGraphEditor::FOnCreateNodeOrPinMenu::CreateSP(this, &SProcedureGraphWidget::CreateNodeMenu);
    Events.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &SProcedureGraphWidget::OnNodeDoubleClicked);
    FGraphAppearanceInfo Appearance;
    Appearance.CornerText = NSLOCTEXT("ProcedureGraph", "Title", "PROCEDURE GRAPH");
    Appearance.InstructionText = NSLOCTEXT("ProcedureGraph", "Hint", "从 Start 沿 Out 连线进入流程；断线保留在当前节点");
    ChildSlot[
        SAssignNew(GraphEditor, SGraphEditor).GraphToEdit(Graph).AdditionalCommands(Commands)
        .GraphEvents(Events).Appearance(Appearance).IsEditable(this, &SProcedureGraphWidget::CanEdit)
    ];
    Rebuild();
}

bool SProcedureGraphWidget::CanEdit() const
{
    return Graph && FProcedureGraphOperations::CanEdit(Graph->Asset);
}

bool SProcedureGraphWidget::CanCopy() const
{
    if (!Graph || !Graph->Asset)
    {
        return false;
    }
    for (const FGuid& Id : SelectedIds)
    {
        if (Graph->Asset->FindNode(Id))
        {
            return true;
        }
    }
    return false;
}

bool SProcedureGraphWidget::HasSelection() const
{
    return CanEdit() && CanCopy();
}

bool SProcedureGraphWidget::CanPaste() const
{
    if (!CanEdit())
    {
        return false;
    }
    FString Text;
    FPlatformApplicationMisc::ClipboardPaste(Text);
    return FProcedureGraphOperations::CanImport(Text);
}

void SProcedureGraphWidget::AddReferencedObjects(FReferenceCollector& Collector)
{
    Collector.AddReferencedObject(Graph);
}

TArray<FGuid> SProcedureGraphWidget::GetSelection() const
{
    return SelectedIds;
}

void SProcedureGraphWidget::Rebuild()
{
    if (!Graph || !Graph->Asset || !GraphEditor)
    {
        return;
    }
    {
        TGuardValue<bool> SelectionGuard(bSelecting, true);
        GraphEditor->ClearSelectionSet();
        Graph->Rebuild();
        SelectedIds.RemoveAll([this](const FGuid& Id) { return !Graph->FindNode(Id); });
        for (const FGuid& Id : SelectedIds)
        {
            if (UProcedureAssetGraphNode* Node = Graph->FindNode(Id))
            {
                GraphEditor->SetNodeSelection(Node, true);
            }
        }
        GraphEditor->NotifyGraphChanged();
    }
    if (ProcedureEditor.IsValid())
    {
        ProcedureEditor.Pin()->RefreshDetails();
    }
}

void SProcedureGraphWidget::OnChanged()
{
    if (!bRebuildQueued)
    {
        bRebuildQueued = true;
        RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SProcedureGraphWidget::RebuildLater));
    }
}

EActiveTimerReturnType SProcedureGraphWidget::RebuildLater(double CurrentTime, float DeltaTime)
{
    bRebuildQueued = false;
    Rebuild();
    return EActiveTimerReturnType::Stop;
}

void SProcedureGraphWidget::OnSelectionChanged(const FGraphPanelSelectionSet& Selection)
{
    if (bSelecting || GIsTransacting)
    {
        return;
    }
    SelectedIds.Reset();
    for (UObject* Object : Selection)
    {
        if (const UProcedureAssetGraphNode* Node = Cast<UProcedureAssetGraphNode>(Object))
        {
            SelectedIds.Add(Node->NodeGuid);
        }
    }
    ProcedureEditor.Pin()->RefreshDetails();
}

void SProcedureGraphWidget::Select(const TArray<FGuid>& Ids)
{
    SelectedIds = Ids;
    Rebuild();
}

void SProcedureGraphWidget::DeleteSelected()
{
    FProcedureGraphOperations::Delete(Graph, SelectedIds);
}

bool SProcedureGraphWidget::WriteClipboard(const FString& Text)
{
    FPlatformApplicationMisc::ClipboardCopy(*Text);
    FString ReadBack;
    FPlatformApplicationMisc::ClipboardPaste(ReadBack);
    return ReadBack == Text;
}

void SProcedureGraphWidget::CopySelected()
{
    FString Text;
    if (FProcedureGraphOperations::Export(Graph->Asset, SelectedIds, Text))
    {
        WriteClipboard(Text);
    }
}

void SProcedureGraphWidget::CutSelected()
{
    FString Text;
    if (HasSelection() && FProcedureGraphOperations::Export(Graph->Asset, SelectedIds, Text) && WriteClipboard(Text))
    {
        DeleteSelected();
    }
}

void SProcedureGraphWidget::Paste()
{
    FString Text;
    FPlatformApplicationMisc::ClipboardPaste(Text);
    const TArray<FGuid> Added = FProcedureGraphOperations::Import(Graph, Text, FVector2D(GraphEditor->GetPasteLocation2f()));
    if (!Added.IsEmpty())
    {
        Select(Added);
    }
}

void SProcedureGraphWidget::DuplicateSelected()
{
    FString Text;
    if (!HasSelection() || !FProcedureGraphOperations::Export(Graph->Asset, SelectedIds, Text))
    {
        return;
    }
    FVector2D Center = FVector2D::ZeroVector;
    int32 Count = 0;
    for (const FGuid& Id : SelectedIds)
    {
        if (const UProcedureBase* Node = Graph->Asset->FindNode(Id))
        {
            Center += Node->GraphPosition;
            ++Count;
        }
    }
    const TArray<FGuid> Added = FProcedureGraphOperations::Import(Graph, Text, Center / FMath::Max(1, Count) + FVector2D(64, 64));
    if (!Added.IsEmpty())
    {
        Select(Added);
    }
}

void SProcedureGraphWidget::AutoLayout()
{
    if (FProcedureGraphOperations::AutoLayout(Graph))
    {
        Rebuild();
        GraphEditor->ZoomToFit(false);
    }
}

void SProcedureGraphWidget::AddCommonMenu(FMenuBuilder& Menu)
{
    FAssetGraphMenu::AddCommon(Menu,
        FUIAction(FExecuteAction::CreateSP(this, &SProcedureGraphWidget::AutoLayout), FCanExecuteAction::CreateSP(this, &SProcedureGraphWidget::CanEdit)),
        FExecuteAction::CreateLambda([this] { GraphEditor->ZoomToFit(false); }),
        FExecuteAction::CreateLambda([this] { Select({}); }));
}

FActionMenuContent SProcedureGraphWidget::CreateActionMenu(UEdGraph* InGraph, const FVector2f& Position, const TArray<UEdGraphPin*>& Pins, bool bAutoExpand, SGraphEditor::FActionMenuClosed OnClosed)
{
    FGuid Id;
    FName Output;
    EEdGraphPinDirection Direction = EGPD_MAX;
    if (!Pins.IsEmpty() && Pins[0])
    {
        Id = Pins[0]->GetOwningNode()->NodeGuid;
        Output = Pins[0]->PinName;
        Direction = Pins[0]->Direction;
    }
    FMenuBuilder Menu(true, Commands);
    Menu.AddSubMenu(NSLOCTEXT("ProcedureGraph", "Create", "Create Procedure Node"), FText::GetEmpty(),
        FNewMenuDelegate::CreateLambda([this, Id, Output, Direction, Position](FMenuBuilder& Sub)
        {
            Sub.AddWidget(CreateClassPicker(false, Id, Direction, Output, FVector2D(Position)), FText::GetEmpty(), true);
        }), FUIAction(FExecuteAction(), FCanExecuteAction::CreateSP(this, &SProcedureGraphWidget::CanEdit)), NAME_None, EUserInterfaceActionType::Button);
    Menu.AddMenuEntry(NSLOCTEXT("ProcedureGraph", "Blueprint", "New Procedure Blueprint"), FText::GetEmpty(), FSlateIcon(),
        FUIAction(FExecuteAction::CreateLambda([] { UEditorStatics::CreateBlueprintAssetWithDialog(UProcedureBlueprintFactory::StaticClass()); }),
        FCanExecuteAction::CreateSP(this, &SProcedureGraphWidget::CanEdit)));
    AddCommonMenu(Menu);
    FActionMenuContent Content(Menu.MakeWidget());
    Content.OnMenuDismissed.AddLambda([OnClosed] { OnClosed.ExecuteIfBound(); });
    return Content;
}

FActionMenuContent SProcedureGraphWidget::CreateNodeMenu(UEdGraph* InGraph, const UEdGraphNode* Node, const UEdGraphPin* Pin, FMenuBuilder* Builder, bool bReadOnly)
{
    const UEdGraphNode* Target = Node ? Node : (Pin ? Pin->GetOwningNode() : nullptr);
    if (!Target || !Builder)
    {
        return FActionMenuContent(SNullWidget::NullWidget);
    }
    const FGuid Id = Target->NodeGuid;
    if (!GraphEditor->GetSelectedNodes().Contains(const_cast<UEdGraphNode*>(Target)))
    {
        TGuardValue<bool> SelectionGuard(bSelecting, true);
        GraphEditor->ClearSelectionSet();
        GraphEditor->SetNodeSelection(const_cast<UEdGraphNode*>(Target), true);
        SelectedIds = {Id};
        ProcedureEditor.Pin()->RefreshDetails();
    }
    if (Pin)
    {
        const FName PinName = Pin->PinName;
        const bool bInput = Pin->Direction == EGPD_Input;
        Builder->AddMenuEntry(FAssetGraphMenu::GetDisconnectPinLabel(), FText::GetEmpty(), FSlateIcon(),
            FUIAction(FExecuteAction::CreateLambda([this, Id, PinName, bInput]
            {
                if (Graph->FindNode(Id))
                {
                    FProcedureGraphOperations::Disconnect(Graph, Id, PinName, bInput);
                }
            }), FCanExecuteAction::CreateSP(this, &SProcedureGraphWidget::CanEdit)));
    }
    Builder->BeginSection("Node", NSLOCTEXT("AssetGraphMenu", "Node", "Node"));
    if (Graph->Asset->FindNode(Id))
    {
        FAssetGraphMenu::AddEditing(*Builder,
            FNewMenuDelegate::CreateLambda([this, Id](FMenuBuilder& Sub)
            {
                Sub.AddWidget(CreateClassPicker(true, Id, EGPD_MAX, NAME_None, FVector2D::ZeroVector), FText::GetEmpty(), true);
            }), FCanExecuteAction::CreateSP(this, &SProcedureGraphWidget::CanEdit),
            FExecuteAction::CreateLambda([this, Id] { OpenSource(Id); }));
    }
    Builder->AddMenuEntry(FAssetGraphMenu::GetDisconnectNodeLabel(), FText::GetEmpty(), FSlateIcon(),
        FUIAction(FExecuteAction::CreateLambda([this, Id] { FProcedureGraphOperations::Disconnect(Graph, Id); }),
        FCanExecuteAction::CreateSP(this, &SProcedureGraphWidget::CanEdit)));
    Builder->EndSection();
    AddCommonMenu(*Builder);
    return FActionMenuContent(Builder->MakeWidget());
}

TSharedRef<SWidget> SProcedureGraphWidget::CreateClassPicker(bool bChangeType, FGuid Id, EEdGraphPinDirection Direction, FName Output, FVector2D Position)
{
    FClassViewerInitializationOptions Options;
    Options.Mode = EClassViewerMode::ClassPicker;
    Options.bShowUnloadedBlueprints = true;
    Options.bShowNoneOption = false;
    Options.ClassFilters.Add(MakeShared<FProcedureClassFilter>());
    return SNew(SBox).WidthOverride(320).HeightOverride(420)[
        FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options,
            FOnClassPicked::CreateSP(this, &SProcedureGraphWidget::OnClassPicked, bChangeType, Id, Direction, Output, Position))
    ];
}

void SProcedureGraphWidget::OnClassPicked(UClass* Class, bool bChangeType, FGuid Id, EEdGraphPinDirection Direction, FName Output, FVector2D Position)
{
    FSlateApplication::Get().DismissAllMenus();
    UProcedureBase* Node = bChangeType ? FProcedureGraphOperations::ChangeType(Graph, Id, Class) :
        FProcedureGraphOperations::Add(Graph, Class, Position, Direction == EGPD_Output ? Id : FGuid(), Output, Direction == EGPD_Input ? Id : FGuid());
    if (Node)
    {
        Select({Node->NodeId});
    }
}

void SProcedureGraphWidget::OpenSource(FGuid Id)
{
    UProcedureBase* Node = Graph->Asset->FindNode(Id);
    if (!Node)
    {
        return;
    }
    if (Node->GetClass()->ClassGeneratedBy)
    {
        GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Node->GetClass()->ClassGeneratedBy);
    }
    else
    {
        FSourceCodeNavigation::NavigateToClass(Node->GetClass());
    }
}

void SProcedureGraphWidget::OnNodeDoubleClicked(UEdGraphNode* Node)
{
    if (Node)
    {
        OpenSource(Node->NodeGuid);
    }
}
