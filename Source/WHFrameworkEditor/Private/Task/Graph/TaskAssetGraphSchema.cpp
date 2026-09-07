#include "Task/Graph/TaskAssetGraphSchema.h"

#include "Task/Graph/TaskAssetGraph.h"
#include "Task/Graph/TaskAssetGraphNode.h"
#include "Task/Graph/TaskGraphOperations.h"
#include "Task/Base/TaskAsset.h"
#include "Task/Base/TaskBase.h"
#include "Task/Base/TaskAssetReferenceTask.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "TaskGraph"

const FPinConnectionResponse UTaskAssetGraphSchema::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
{
	if (!A || !B || A->Direction == B->Direction || A->GetOwningNode()->GetGraph() != B->GetOwningNode()->GetGraph())
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("Direction", "Connect Out to In."));
	const UTaskAssetGraphNode* Parent = Cast<UTaskAssetGraphNode>((A->Direction == EGPD_Output ? A : B)->GetOwningNode());
	const UTaskAssetGraphNode* Child = Cast<UTaskAssetGraphNode>((A->Direction == EGPD_Input ? A : B)->GetOwningNode());
	if (!Parent || !Child || !Parent->Task || !Child->Task || Parent == Child || Child->Task->IsParentOf(Parent->Task))
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("Cycle", "A task cannot contain itself or an ancestor."));
	if(Parent->Task->IsA<UTaskAssetReferenceTask>())
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ReferenceChildren", "Task asset reference nodes receive children from their referenced asset at runtime."));
	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, LOCTEXT("Reparent", "Set parent task."));
}

bool UTaskAssetGraphSchema::TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const
{
	if (CanCreateConnection(A, B).Response == CONNECT_RESPONSE_DISALLOW) return false;
	UTaskBase* Parent = CastChecked<UTaskAssetGraphNode>((A->Direction == EGPD_Output ? A : B)->GetOwningNode())->Task;
	UTaskBase* Child = CastChecked<UTaskAssetGraphNode>((A->Direction == EGPD_Input ? A : B)->GetOwningNode())->Task;
	UTaskAssetGraph* Graph = CastChecked<UTaskAssetGraph>(A->GetOwningNode()->GetGraph());
	if (!FTaskGraphOperations::CanEdit(Graph->TaskAsset) || Child->ParentTask == Parent) return false;
	const FScopedTransaction Transaction(LOCTEXT("Connect", "Connect Tasks"));
	Graph->TaskAsset->Modify();
	for (const auto& Pair : Graph->TaskAsset->TaskMap) Pair.Value->Modify();
	Parent->Modify();
	Child->Modify();
	if (Child->ParentTask)
	{
		Child->ParentTask->Modify();
		Child->ParentTask->SubTasks.Remove(Child);
	}
	Graph->TaskAsset->RootTasks.Remove(Child);
	Parent->SubTasks.Add(Child);
	Graph->TaskAsset->RebuildTaskMap(false);
	FTaskGraphOperations::SortSiblings(Graph->TaskAsset, Child);
	Graph->OnTasksChanged.ExecuteIfBound();
	return true;
}

void UTaskAssetGraphSchema::BreakPinLinks(UEdGraphPin& Pin, bool bSendsNodeNotification) const
{
	UTaskAssetGraph* Graph = CastChecked<UTaskAssetGraph>(Pin.GetOwningNode()->GetGraph());
	UTaskBase* Task = CastChecked<UTaskAssetGraphNode>(Pin.GetOwningNode())->Task;
	if (!FTaskGraphOperations::CanEdit(Graph->TaskAsset) || (Pin.Direction == EGPD_Input ? !Task->ParentTask : Task->SubTasks.IsEmpty())) return;
	const FScopedTransaction Transaction(LOCTEXT("Disconnect", "Disconnect Tasks"));
	Graph->TaskAsset->Modify();
	for (const auto& Pair : Graph->TaskAsset->TaskMap) Pair.Value->Modify();
	Task->Modify();
	UTaskBase* SortTask = Task;
	if (Pin.Direction == EGPD_Input && Task->ParentTask)
	{
		Task->ParentTask->Modify();
		Task->ParentTask->SubTasks.Remove(Task);
		Graph->TaskAsset->RootTasks.AddUnique(Task);
	}
	else if (Pin.Direction == EGPD_Output)
	{
		if(!Task->SubTasks.IsEmpty()) SortTask = Task->SubTasks[0];
		for (UTaskBase* Child : Task->SubTasks)
		{
			Child->Modify();
			Graph->TaskAsset->RootTasks.AddUnique(Child);
		}
		Task->SubTasks.Reset();
	}
	Graph->TaskAsset->RebuildTaskMap(false);
	FTaskGraphOperations::SortSiblings(Graph->TaskAsset, SortTask);
	Graph->OnTasksChanged.ExecuteIfBound();
}

void UTaskAssetGraphSchema::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
	UTaskAssetGraph* Graph = CastChecked<UTaskAssetGraph>(TargetNode.GetGraph());
	UTaskBase* Task = CastChecked<UTaskAssetGraphNode>(&TargetNode)->Task;
	if (!FTaskGraphOperations::CanEdit(Graph->TaskAsset) || (!Task->ParentTask && Task->SubTasks.IsEmpty())) return;
	const FScopedTransaction Transaction(LOCTEXT("DisconnectNode", "Disconnect Task Node"));
	Graph->TaskAsset->Modify();
	for (const auto& Pair : Graph->TaskAsset->TaskMap) Pair.Value->Modify();
	if (Task->ParentTask)
	{
		Task->ParentTask->SubTasks.Remove(Task);
		Graph->TaskAsset->RootTasks.AddUnique(Task);
	}
	for (UTaskBase* Child : Task->SubTasks) Graph->TaskAsset->RootTasks.AddUnique(Child);
	Task->SubTasks.Reset();
	Graph->TaskAsset->RebuildTaskMap(false);
	FTaskGraphOperations::SortSiblings(Graph->TaskAsset, Task);
	Graph->OnTasksChanged.ExecuteIfBound();
}

void UTaskAssetGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
	if (SourcePin && TargetPin && SourcePin->LinkedTo.Contains(TargetPin))
	{
		BreakPinLinks(*(SourcePin->Direction == EGPD_Input ? SourcePin : TargetPin), true);
	}
}

FLinearColor UTaskAssetGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return FLinearColor(0.25f, 0.65f, 0.9f);
}

#undef LOCTEXT_NAMESPACE
