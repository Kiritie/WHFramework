#pragma once

#include "Widgets/SCompoundWidget.h"
#include "UObject/GCObject.h"
#include "GraphEditor.h"

class FTaskEditor;
class UTaskBase;
class UTaskAssetGraph;

class WHFRAMEWORKEDITOR_API STaskGraphWidget : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(STaskGraphWidget) {}
		SLATE_ARGUMENT(TWeakPtr<FTaskEditor>, TaskEditor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void Rebuild();
	void SelectTasks(const TArray<UTaskBase*>& Tasks);
	TArray<UTaskBase*> GetSelectedTasks() const;
	bool CanEdit() const;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("STaskGraphWidget"); }
	TSharedPtr<FUICommandList> GetCommands() const { return Commands; }
	TSharedPtr<SGraphEditor> GetGraphEditor() const { return GraphEditor; }
	FActionMenuContent CreateActionMenu(UEdGraph* InGraph, const FVector2f& Position, const TArray<UEdGraphPin*>& Pins, bool bAutoExpand, SGraphEditor::FActionMenuClosed OnClosed);
	FActionMenuContent CreateNodeMenu(UEdGraph* InGraph, const UEdGraphNode* Node, const UEdGraphPin* Pin, FMenuBuilder* Builder, bool bReadOnly);

private:
	void OnSelectionChanged(const FGraphPanelSelectionSet& Selection);
	void OnTasksChanged();
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
	TSharedRef<SWidget> CreateClassPicker(bool bChangeType, TWeakObjectPtr<UTaskBase> Task, EEdGraphPinDirection Direction, FVector2D Position);
	void OnClassPicked(UClass* Class, bool bChangeType, TWeakObjectPtr<UTaskBase> Task, EEdGraphPinDirection Direction, FVector2D Position);
	void EditTaskSource(UTaskBase* Task);
	void OnNodeDoubleClicked(UEdGraphNode* Node);

	TWeakPtr<FTaskEditor> TaskEditor;
	TObjectPtr<UTaskAssetGraph> Graph = nullptr;
	TSharedPtr<SGraphEditor> GraphEditor;
	TSharedPtr<FUICommandList> Commands;
	mutable double LastClipboardCheckTime = -1.;
	mutable bool bCanPasteCached = false;
	TSet<FString> SelectedTaskIDs;
	bool bSelecting = false;
};
