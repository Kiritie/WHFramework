#include "Task/Graph/TaskAssetGraphNode.h"

#include "Task/Base/TaskBase.h"
#include "Task/Base/TaskAsset.h"
#include "Task/Base/TaskAssetReferenceTask.h"
#include "Task/Graph/TaskAssetGraph.h"
#include "Task/Graph/TaskGraphOperations.h"
#include "EdGraphUtilities.h"
#include "SGraphNodeDefault.h"

namespace
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
	CreatePin(EGPD_Input, TEXT("Task"), TEXT("In"));
	CreatePin(EGPD_Output, TEXT("Task"), TEXT("Out"));
}

FText UTaskAssetGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if(!Task) return FText::GetEmpty();
	const FText Order = FText::FromString(GetTaskOrderLabel(Task));
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
	if(const UTaskAssetReferenceTask* ReferenceTask = Cast<UTaskAssetReferenceTask>(Task))
	{
		return FText::Format(NSLOCTEXT("TaskGraph", "ReferenceTooltip", "{0}\nReferenced asset: {1}\nDouble-click to open the referenced task graph."),
			Task->TaskDescription, FText::FromString(ReferenceTask->ReferencedAsset.ToSoftObjectPath().ToString()));
	}
	return Task ? Task->TaskDescription : FText::GetEmpty();
}

FLinearColor UTaskAssetGraphNode::GetNodeTitleColor() const
{
	if (!Task) return FLinearColor::Gray;
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
