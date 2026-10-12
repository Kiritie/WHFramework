#pragma once

#include "Widgets/SCompoundWidget.h"
#include "UObject/GCObject.h"
#include "GraphEditor.h"

class FProcedureEditor;
class UProcedureAssetGraph;

class WHFRAMEWORKEDITOR_API SProcedureGraphWidget : public SCompoundWidget, public FGCObject
{
public:
    SLATE_BEGIN_ARGS(SProcedureGraphWidget) {}
        SLATE_ARGUMENT(TWeakPtr<FProcedureEditor>, ProcedureEditor)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void Rebuild();
    void Select(const TArray<FGuid>& Ids);
    TArray<FGuid> GetSelection() const;
    bool CanEdit() const;
    UProcedureAssetGraph* GetGraph() const { return Graph; }
    virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
    virtual FString GetReferencerName() const override { return TEXT("SProcedureGraphWidget"); }
    TSharedPtr<FUICommandList> GetCommands() const { return Commands; }
    FActionMenuContent CreateActionMenu(UEdGraph* InGraph, const FVector2f& Position, const TArray<UEdGraphPin*>& Pins, bool bAutoExpand, SGraphEditor::FActionMenuClosed OnClosed);
    FActionMenuContent CreateNodeMenu(UEdGraph* InGraph, const UEdGraphNode* Node, const UEdGraphPin* Pin, FMenuBuilder* Builder, bool bReadOnly);

private:
    void OnSelectionChanged(const FGraphPanelSelectionSet& Selection);
    void OnChanged();
    EActiveTimerReturnType RebuildLater(double CurrentTime, float DeltaTime);
    void DeleteSelected();
    void CopySelected();
    void CutSelected();
    void Paste();
    void DuplicateSelected();
    void AutoLayout();
    bool HasSelection() const;
    bool CanCopy() const;
    bool CanPaste() const;
    void AddCommonMenu(FMenuBuilder& Menu);
    TSharedRef<SWidget> CreateClassPicker(bool bChangeType, FGuid Id, EEdGraphPinDirection Direction, FName Output, FVector2D Position);
    void OnClassPicked(UClass* Class, bool bChangeType, FGuid Id, EEdGraphPinDirection Direction, FName Output, FVector2D Position);
    void OpenSource(FGuid Id);
    void OnNodeDoubleClicked(UEdGraphNode* Node);
    bool WriteClipboard(const FString& Text);

    TWeakPtr<FProcedureEditor> ProcedureEditor;
    TObjectPtr<UProcedureAssetGraph> Graph;
    TSharedPtr<SGraphEditor> GraphEditor;
    TSharedPtr<FUICommandList> Commands;
    TArray<FGuid> SelectedIds;
    bool bSelecting = false;
    bool bRebuildQueued = false;
};
