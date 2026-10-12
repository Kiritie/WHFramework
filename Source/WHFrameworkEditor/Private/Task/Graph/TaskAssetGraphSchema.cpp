#include "Task/Graph/TaskAssetGraphSchema.h"

#include "Task/Graph/TaskAssetGraph.h"
#include "Task/Graph/TaskAssetGraphNode.h"
#include "Task/Graph/TaskGraphOperations.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "TaskGraph"

const FPinConnectionResponse UTaskAssetGraphSchema::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
{
	if(!A || !B || A->Direction == B->Direction || A->PinType.PinCategory != TEXT("Task") || B->PinType.PinCategory != TEXT("Task") ||
		A->GetOwningNode()->GetGraph() != B->GetOwningNode()->GetGraph())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("Direction", "Connect Out to In in the same graph."));
	}
	const UTaskAssetGraphNode* Parent = Cast<UTaskAssetGraphNode>((A->Direction == EGPD_Output ? A : B)->GetOwningNode());
	const UTaskAssetGraphNode* Child = Cast<UTaskAssetGraphNode>((A->Direction == EGPD_Input ? A : B)->GetOwningNode());
	if(!Parent || !Child || Child->bStartNode)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("StartInput", "Start has no input."));
	}
	const UTaskAssetGraph* Graph = Cast<UTaskAssetGraph>(Parent->GetGraph());
	FText Reason;
	const bool bAllowed = Graph && FTaskGraphOperations::CanConnect(Graph->TaskAsset, Parent->Task, Child->Task, Parent->bStartNode, Reason);
	return FPinConnectionResponse(bAllowed ? CONNECT_RESPONSE_MAKE : CONNECT_RESPONSE_DISALLOW, Reason);
}

bool UTaskAssetGraphSchema::TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const
{
	if(CanCreateConnection(A, B).Response == CONNECT_RESPONSE_DISALLOW)
	{
		return false;
	}
	UTaskAssetGraphNode* Parent = CastChecked<UTaskAssetGraphNode>((A->Direction == EGPD_Output ? A : B)->GetOwningNode());
	UTaskAssetGraphNode* Child = CastChecked<UTaskAssetGraphNode>((A->Direction == EGPD_Input ? A : B)->GetOwningNode());
	UTaskAssetGraph* Graph = CastChecked<UTaskAssetGraph>(Parent->GetGraph());
	const FScopedTransaction Transaction(LOCTEXT("Connect", "Connect Tasks"));
	Graph->Modify();
	Parent->Modify();
	Child->Modify();
	const bool bChanged = FTaskGraphOperations::Connect(Graph->TaskAsset, Parent->Task, Child->Task, Parent->bStartNode);
	if(bChanged)
	{
		Graph->OnTasksChanged.ExecuteIfBound();
	}
	return bChanged;
}

void UTaskAssetGraphSchema::BreakPinLinks(UEdGraphPin& Pin, bool bSendsNodeNotification) const
{
	UTaskAssetGraph* Graph = Cast<UTaskAssetGraph>(Pin.GetOwningNode()->GetGraph());
	UTaskAssetGraphNode* Node = Cast<UTaskAssetGraphNode>(Pin.GetOwningNode());
	if(!Graph || !Node || Pin.LinkedTo.IsEmpty() || !FTaskGraphOperations::CanEdit(Graph->TaskAsset))
	{
		return;
	}
	const FScopedTransaction Transaction(LOCTEXT("Disconnect", "Disconnect Tasks"));
	Graph->Modify();
	Node->Modify();
	for(UEdGraphPin* Other : Pin.LinkedTo)
	{
		Other->GetOwningNode()->Modify();
	}
	if(FTaskGraphOperations::Disconnect(Graph->TaskAsset, Node->Task, Pin.Direction == EGPD_Input, Pin.Direction == EGPD_Output, Node->bStartNode))
	{
		Graph->OnTasksChanged.ExecuteIfBound();
	}
}

void UTaskAssetGraphSchema::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
	UTaskAssetGraph* Graph = Cast<UTaskAssetGraph>(TargetNode.GetGraph());
	UTaskAssetGraphNode* Node = Cast<UTaskAssetGraphNode>(&TargetNode);
	if(!Graph || !Node || !FTaskGraphOperations::CanEdit(Graph->TaskAsset))
	{
		return;
	}
	bool bHasLinks = false;
	for(UEdGraphPin* Pin : Node->Pins)
	{
		bHasLinks |= !Pin->LinkedTo.IsEmpty();
	}
	if(!bHasLinks)
	{
		return;
	}
	const FScopedTransaction Transaction(LOCTEXT("DisconnectNode", "Disconnect Task Node"));
	Graph->Modify();
	Node->Modify();
	for(UEdGraphPin* Pin : Node->Pins)
	{
		for(UEdGraphPin* Other : Pin->LinkedTo)
		{
			Other->GetOwningNode()->Modify();
		}
	}
	if(FTaskGraphOperations::Disconnect(Graph->TaskAsset, Node->Task, true, true, Node->bStartNode))
	{
		Graph->OnTasksChanged.ExecuteIfBound();
	}
}

void UTaskAssetGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
	if(!SourcePin || !TargetPin || !SourcePin->LinkedTo.Contains(TargetPin))
	{
		return;
	}
	UTaskAssetGraphNode* Parent = Cast<UTaskAssetGraphNode>((SourcePin->Direction == EGPD_Output ? SourcePin : TargetPin)->GetOwningNode());
	UTaskAssetGraphNode* Child = Cast<UTaskAssetGraphNode>((SourcePin->Direction == EGPD_Input ? SourcePin : TargetPin)->GetOwningNode());
	UTaskAssetGraph* Graph = Parent ? Cast<UTaskAssetGraph>(Parent->GetGraph()) : nullptr;
	if(!Graph || !Child || !FTaskGraphOperations::CanEdit(Graph->TaskAsset))
	{
		return;
	}
	const FScopedTransaction Transaction(LOCTEXT("DisconnectEdge", "Disconnect Task Link"));
	Graph->Modify();
	Parent->Modify();
	Child->Modify();
	if(FTaskGraphOperations::DisconnectEdge(Graph->TaskAsset, Parent->Task, Child->Task, Parent->bStartNode))
	{
		Graph->OnTasksChanged.ExecuteIfBound();
	}
}

FLinearColor UTaskAssetGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return FLinearColor(0.25f, 0.65f, 0.9f);
}

#undef LOCTEXT_NAMESPACE
