#include "Task/Graph/TaskAssetGraphNode.h"

#include "Task/Base/TaskBase.h"
#include "Task/Base/TaskAsset.h"
#include "Task/Base/TaskAssetReferenceTask.h"
#include "Task/Graph/TaskAssetGraph.h"
#include "Task/Graph/TaskGraphOperations.h"
#include "EdGraphUtilities.h"
#include "SGraphNodeDefault.h"

namespace TaskGraphNodes
{
	FString GetTaskOrderLabel(const UTaskBase* Task)
	{
		const UTaskAsset* Asset = Task ? Task->GetTaskAsset() : nullptr;
		if(!Task || !Asset) return FString();
		TArray<int32> Segments;
		for(const UTaskBase* Item = Task; Item; Item = Item->ParentTask)
		{
			const TArray<UTaskBase*>& Siblings = Item->ParentTask ? Item->ParentTask->SubTasks : Asset->RootTasks;
			const int32 Index = Siblings.IndexOfByKey(Item);
			if(Index == INDEX_NONE) return FString();
			Segments.Insert(Index + 1, 0);
		}
		TArray<FString> Parts;
		for(const int32 Segment : Segments) Parts.Add(FString::FromInt(Segment));
		return FString::Join(Parts, TEXT("."));
	}
}

void UTaskAssetGraphNode::AllocateDefaultPins()
{
	if(!bStartNode)
	{
		CreatePin(EGPD_Input, TEXT("Task"), TEXT("In"));
	}
	CreatePin(EGPD_Output, TEXT("Task"), TEXT("Out"));
}

FText UTaskAssetGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if(bStartNode)
	{
		return NSLOCTEXT("TaskGraph", "Start", "Start");
	}
	if(!Task)
	{
		return FText::GetEmpty();
	}
	if(!Task->GetTaskAsset()->TaskMap.Contains(Task->TaskGUID))
	{
		return FText::Format(NSLOCTEXT("TaskGraph", "DisconnectedTitle", "[Disconnected] {0}"), Task->TaskDisplayName);
	}
	const FText Order = FText::FromString(TaskGraphNodes::GetTaskOrderLabel(Task));
	if(const UTaskAssetReferenceTask* ReferenceTask = Cast<UTaskAssetReferenceTask>(Task))
	{
		const FString AssetName = ReferenceTask->ReferencedAsset.IsNull() ? TEXT("None") : ReferenceTask->ReferencedAsset.ToSoftObjectPath().GetAssetName();
		return FText::Format(NSLOCTEXT("TaskGraph", "OrderedReferenceTitle", "[{0}] {1}  [{2}]"),
			Order, Task->TaskDisplayName, FText::FromString(AssetName));
	}
	return FText::Format(NSLOCTEXT("TaskGraph", "OrderedTaskTitle", "[{0}] {1}"), Order, Task->TaskDisplayName);
}

FText UTaskAssetGraphNode::GetTooltipText() const
{
	if(bStartNode)
	{
		return NSLOCTEXT("TaskGraph", "StartTooltip", "Only tasks reachable from Start enter the compiled task tree.");
	}
	if(const UTaskAssetReferenceTask* ReferenceTask = Cast<UTaskAssetReferenceTask>(Task))
	{
		return FText::Format(NSLOCTEXT("TaskGraph", "ReferenceTooltip", "{0}\nReferenced asset: {1}\nDouble-click to open the referenced task graph."),
			Task->TaskDescription, FText::FromString(ReferenceTask->ReferencedAsset.ToSoftObjectPath().ToString()));
	}
	return Task ? Task->TaskDescription : FText::GetEmpty();
}

FLinearColor UTaskAssetGraphNode::GetNodeTitleColor() const
{
	if(bStartNode)
	{
		return FLinearColor(0.1f, 0.55f, 0.25f);
	}
	if(!Task || !Task->GetTaskAsset()->TaskMap.Contains(Task->TaskGUID))
	{
		return FLinearColor::Gray;
	}
	if(Task->IsA<UTaskAssetReferenceTask>()) return FLinearColor(0.12f, 0.48f, 0.52f);
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
		UTaskAssetGraph* Graph = CastChecked<UTaskAssetGraph>(Node->GetGraph());
		const FVector2D Position(FMath::RoundToInt(NewPosition.X), FMath::RoundToInt(NewPosition.Y));
		const FVector2D Previous = Node->bStartNode ? Graph->TaskAsset->StartPosition : Node->Task->GraphPosition;
		if(bMarkDirty && Position != Previous)
		{
			if(!FTaskGraphOperations::CanEdit(Graph->TaskAsset))
			{
				return;
			}
			Graph->Modify();
			Node->Modify();
			FTaskGraphOperations::SetNodePosition(Graph->TaskAsset, Node->Task, Node->bStartNode, Position);
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


FString UTaskAssetGraphNode::GetStableGUID() const
{
	return bStartNode ? NodeGuid.ToString() : Task ? Task->TaskGUID : FString();
}

bool UTaskAssetGraphNode::CanUserDeleteNode() const
{
	return !bStartNode && Task != nullptr;
}

bool UTaskAssetGraphNode::CanDuplicateNode() const
{
	return !bStartNode && Task != nullptr;
}
