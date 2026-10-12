#include "Step/Graph/StepAssetGraphSchema.h"

#include "Step/Graph/StepAssetGraph.h"
#include "Step/Graph/StepAssetGraphNode.h"
#include "Step/Graph/StepGraphOperations.h"

const FPinConnectionResponse UStepAssetGraphSchema::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
{
	if(!A || !B || A->Direction == B->Direction || A->GetOwningNode()->GetGraph() != B->GetOwningNode()->GetGraph() || A->PinType.PinCategory != TEXT("Step") || B->PinType.PinCategory != TEXT("Step"))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, NSLOCTEXT("StepGraph", "Direction", "Connect Out to In in the same graph."));
	}
	const UStepAssetGraphNode* Parent = Cast<UStepAssetGraphNode>((A->Direction == EGPD_Output ? A : B)->GetOwningNode());
	const UStepAssetGraphNode* Child = Cast<UStepAssetGraphNode>((A->Direction == EGPD_Input ? A : B)->GetOwningNode());
	const UStepAssetGraph* Graph = Cast<UStepAssetGraph>(A->GetOwningNode()->GetGraph());
	FText Reason;
	if(!Parent || !Child || !Graph || !FStepGraphOperations::CanEdit(Graph->StepAsset) || !FStepGraphOperations::CanConnect(Graph->StepAsset, Parent->StableGUID, Child->StableGUID, Reason))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, Reason);
	}
	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, NSLOCTEXT("StepGraph", "Connect", "Connect parent to child."));
}

bool UStepAssetGraphSchema::TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const
{
	if(CanCreateConnection(A, B).Response == CONNECT_RESPONSE_DISALLOW)
	{
		return false;
	}
	UStepAssetGraph* Graph = CastChecked<UStepAssetGraph>(A->GetOwningNode()->GetGraph());
	const FString ParentGUID = CastChecked<UStepAssetGraphNode>((A->Direction == EGPD_Output ? A : B)->GetOwningNode())->StableGUID;
	const FString ChildGUID = CastChecked<UStepAssetGraphNode>((A->Direction == EGPD_Input ? A : B)->GetOwningNode())->StableGUID;
	if(FStepGraphOperations::Connect(Graph->StepAsset, ParentGUID, ChildGUID))
	{
		Graph->OnStepsChanged.ExecuteIfBound();
		return true;
	}
	return false;
}

void UStepAssetGraphSchema::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
	UStepAssetGraphNode* Node = Cast<UStepAssetGraphNode>(&TargetNode);
	UStepAssetGraph* Graph = Node ? Cast<UStepAssetGraph>(Node->GetGraph()) : nullptr;
	if(Graph && FStepGraphOperations::Disconnect(Graph->StepAsset, Node->StableGUID, true, true))
	{
		Graph->OnStepsChanged.ExecuteIfBound();
	}
}

void UStepAssetGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const
{
	UStepAssetGraphNode* Node = Cast<UStepAssetGraphNode>(TargetPin.GetOwningNode());
	UStepAssetGraph* Graph = Node ? Cast<UStepAssetGraph>(Node->GetGraph()) : nullptr;
	if(Graph && FStepGraphOperations::Disconnect(Graph->StepAsset, Node->StableGUID, TargetPin.Direction == EGPD_Input, TargetPin.Direction == EGPD_Output))
	{
		Graph->OnStepsChanged.ExecuteIfBound();
	}
}

void UStepAssetGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
	if(!SourcePin || !TargetPin || SourcePin->Direction == TargetPin->Direction || !SourcePin->LinkedTo.Contains(TargetPin))
	{
		return;
	}
	UStepAssetGraph* Graph = Cast<UStepAssetGraph>(SourcePin->GetOwningNode()->GetGraph());
	const UStepAssetGraphNode* Parent = Cast<UStepAssetGraphNode>((SourcePin->Direction == EGPD_Output ? SourcePin : TargetPin)->GetOwningNode());
	const UStepAssetGraphNode* Child = Cast<UStepAssetGraphNode>((SourcePin->Direction == EGPD_Input ? SourcePin : TargetPin)->GetOwningNode());
	if(Graph && Parent && Child && FStepGraphOperations::DisconnectEdge(Graph->StepAsset, Parent->StableGUID, Child->StableGUID))
	{
		Graph->OnStepsChanged.ExecuteIfBound();
	}
}

FLinearColor UStepAssetGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return FLinearColor(0.25f, 0.65f, 0.9f);
}
