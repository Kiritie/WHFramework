#include "Task/Graph/TaskAssetGraphNode.h"

#include "Task/Base/TaskBase.h"
#include "Task/Graph/TaskAssetGraph.h"
#include "Task/Graph/TaskGraphOperations.h"
#include "EdGraphUtilities.h"
#include "SGraphNodeDefault.h"

void UTaskAssetGraphNode::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, TEXT("Task"), TEXT("In"));
	CreatePin(EGPD_Output, TEXT("Task"), TEXT("Out"));
}

FText UTaskAssetGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return Task ? Task->TaskDisplayName : FText::GetEmpty();
}

FText UTaskAssetGraphNode::GetTooltipText() const
{
	return Task ? Task->TaskDescription : FText::GetEmpty();
}

FLinearColor UTaskAssetGraphNode::GetNodeTitleColor() const
{
	if (!Task) return FLinearColor::Gray;
	switch (Task->TaskState)
	{
		case ETaskState::Entered: return FLinearColor(0.1f, 0.45f, 0.8f);
		case ETaskState::Executing: return FLinearColor(0.8f, 0.5f, 0.08f);
		case ETaskState::Completed: return FLinearColor(0.1f, 0.6f, 0.25f);
		case ETaskState::Leaved: return FLinearColor::Gray;
		default: return FLinearColor(0.18f, 0.22f, 0.4f);
	}
}

class STaskAssetGraphNode : public SGraphNodeDefault
{
public:
	SLATE_BEGIN_ARGS(STaskAssetGraphNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UTaskAssetGraphNode* InNode)
	{
		SGraphNodeDefault::Construct(SGraphNodeDefault::FArguments().GraphNodeObj(InNode));
	}

	virtual void MoveTo(const FVector2f& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty = true) override
	{
		UTaskAssetGraphNode* Node = CastChecked<UTaskAssetGraphNode>(GraphNode);
		if (NodeFilter.Contains(SharedThis(this))) return;
		if (Node->Task && bMarkDirty)
		{
			UTaskAssetGraph* Graph = CastChecked<UTaskAssetGraph>(Node->GetGraph());
			if (!FTaskGraphOperations::CanEdit(Graph->TaskAsset)) return;
			for (UEdGraphNode* Item : Graph->Nodes)
			{
				UTaskAssetGraphNode* Other = CastChecked<UTaskAssetGraphNode>(Item);
				if (Other->Task->ParentTask == Node->Task->ParentTask && !Other->Task->bHasGraphPosition)
				{
					Other->Task->Modify();
					Other->Task->GraphPosition = FVector2D(Other->NodePosX, Other->NodePosY);
					Other->Task->bHasGraphPosition = true;
				}
			}
			Node->Task->Modify();
			Node->Task->GraphPosition = FVector2D(NewPosition);
			Node->Task->bHasGraphPosition = true;
			FTaskGraphOperations::SortSiblings(Graph->TaskAsset, Node->Task);
		}
		SGraphNodeDefault::MoveTo(NewPosition, NodeFilter, bMarkDirty);
	}
};

class FTaskAssetGraphNodeFactory : public FGraphPanelNodeFactory
{
public:
	virtual TSharedPtr<SGraphNode> CreateNode(UEdGraphNode* Node) const override
	{
		if (UTaskAssetGraphNode* TaskNode = Cast<UTaskAssetGraphNode>(Node))
		{
			return SNew(STaskAssetGraphNode, TaskNode);
		}
		return nullptr;
	}
};

TSharedRef<FGraphPanelNodeFactory> CreateTaskGraphNodeFactory()
{
	return MakeShared<FTaskAssetGraphNodeFactory>();
}
