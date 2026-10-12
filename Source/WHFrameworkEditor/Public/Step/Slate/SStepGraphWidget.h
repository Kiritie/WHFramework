#pragma once

#include "Widgets/SCompoundWidget.h"
#include "UObject/GCObject.h"
#include "GraphEditor.h"

class FStepEditor;
class UStepBase;
class UStepAssetGraph;

class WHFRAMEWORKEDITOR_API SStepGraphWidget : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SStepGraphWidget) {}
		SLATE_ARGUMENT(TWeakPtr<FStepEditor>, StepEditor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void Rebuild();
	void SelectSteps(const TArray<UStepBase*>& Steps);
	TArray<UStepBase*> GetSelectedSteps() const;
	bool CanEdit() const;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SStepGraphWidget"); }
	TSharedPtr<FUICommandList> GetCommands() const { return Commands; }
	TSharedPtr<SGraphEditor> GetGraphEditor() const { return GraphEditor; }
	UStepAssetGraph* GetGraph() const { return Graph; }
	FActionMenuContent CreateActionMenu(UEdGraph* InGraph, const FVector2f& Position, const TArray<UEdGraphPin*>& Pins, bool bAutoExpand, SGraphEditor::FActionMenuClosed OnClosed);
	FActionMenuContent CreateNodeMenu(UEdGraph* InGraph, const UEdGraphNode* Node, const UEdGraphPin* Pin, FMenuBuilder* Builder, bool bReadOnly);

private:
	void OnSelectionChanged(const FGraphPanelSelectionSet& Selection);
	void OnStepsChanged();
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
	TSharedRef<SWidget> CreateClassPicker(bool bChangeType, FString NodeGUID, EEdGraphPinDirection Direction, FVector2D Position);
	void OnClassPicked(UClass* Class, bool bChangeType, FString NodeGUID, EEdGraphPinDirection Direction, FVector2D Position);
	void EditStepSource(const FString& NodeGUID);
	void OnNodeDoubleClicked(UEdGraphNode* Node);

	TWeakPtr<FStepEditor> StepEditor;
	TObjectPtr<UStepAssetGraph> Graph = nullptr;
	TSharedPtr<SGraphEditor> GraphEditor;
	TSharedPtr<FUICommandList> Commands;
	mutable double LastClipboardCheckTime = -1.;
	mutable bool bCanPasteCached = false;
	TSet<FString> SelectedStepIDs;
	bool bSelecting = false;
	bool bRebuildQueued = false;
};
