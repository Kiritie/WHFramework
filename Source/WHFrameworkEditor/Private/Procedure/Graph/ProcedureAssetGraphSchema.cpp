#include "Procedure/Graph/ProcedureAssetGraphSchema.h"

#include "Procedure/Graph/ProcedureAssetGraph.h"
#include "Procedure/Graph/ProcedureAssetGraphNode.h"
#include "Procedure/Graph/ProcedureGraphOperations.h"

const FPinConnectionResponse UProcedureAssetGraphSchema::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
{
    if (!A || !B || A->Direction == B->Direction || A->GetOwningNode()->GetGraph() != B->GetOwningNode()->GetGraph())
    {
        return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, NSLOCTEXT("ProcedureGraph", "Direction", "仅允许 Out 连接 In"));
    }
    const UEdGraphPin* Output = A->Direction == EGPD_Output ? A : B;
    const UEdGraphPin* Input = A->Direction == EGPD_Input ? A : B;
    const UProcedureAssetGraph* Graph = Cast<UProcedureAssetGraph>(Output->GetOwningNode()->GetGraph());
    if (!Graph || !FProcedureGraphOperations::CanEdit(Graph->Asset) ||
        !FProcedureGraphOperations::CanConnect(Graph->Asset, Output->GetOwningNode()->NodeGuid, Output->PinName, Input->GetOwningNode()->NodeGuid))
    {
        return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, NSLOCTEXT("ProcedureGraph", "Invalid", "Start 无输入，默认链不允许环；已占用端口请先断开"));
    }
    return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, NSLOCTEXT("ProcedureGraph", "Replace", "将空闲 Out 连接到空闲 In"));
}

bool UProcedureAssetGraphSchema::TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const
{
    if (CanCreateConnection(A, B).Response == CONNECT_RESPONSE_DISALLOW)
    {
        return false;
    }
    UEdGraphPin* Output = A->Direction == EGPD_Output ? A : B;
    UEdGraphPin* Input = A->Direction == EGPD_Input ? A : B;
    UProcedureAssetGraph* Graph = CastChecked<UProcedureAssetGraph>(Output->GetOwningNode()->GetGraph());
    return FProcedureGraphOperations::Connect(Graph, Output->GetOwningNode()->NodeGuid, Output->PinName, Input->GetOwningNode()->NodeGuid);
}

void UProcedureAssetGraphSchema::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
    if (UProcedureAssetGraph* Graph = Cast<UProcedureAssetGraph>(TargetNode.GetGraph()))
    {
        FProcedureGraphOperations::Disconnect(Graph, TargetNode.NodeGuid);
    }
}

void UProcedureAssetGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const
{
    if (UProcedureAssetGraph* Graph = Cast<UProcedureAssetGraph>(TargetPin.GetOwningNode()->GetGraph()))
    {
        FProcedureGraphOperations::Disconnect(Graph, TargetPin.GetOwningNode()->NodeGuid, TargetPin.PinName, TargetPin.Direction == EGPD_Input);
    }
}

void UProcedureAssetGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
    if (!SourcePin || !TargetPin || !SourcePin->LinkedTo.Contains(TargetPin))
    {
        return;
    }
    UEdGraphPin* Output = SourcePin->Direction == EGPD_Output ? SourcePin : TargetPin;
    UEdGraphPin* Input = SourcePin->Direction == EGPD_Input ? SourcePin : TargetPin;
    if (UProcedureAssetGraph* Graph = Cast<UProcedureAssetGraph>(Output->GetOwningNode()->GetGraph()))
    {
        FProcedureGraphOperations::Disconnect(Graph, Output->GetOwningNode()->NodeGuid, Output->PinName, false, Input->GetOwningNode()->NodeGuid);
    }
}

FLinearColor UProcedureAssetGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
    return FLinearColor(0.2f, 0.65f, 0.95f);
}
