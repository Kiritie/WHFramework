#include "Procedure/Graph/ProcedureAssetGraph.h"

#include "Procedure/Base/ProcedureAsset.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Procedure/Graph/ProcedureAssetGraphNode.h"

UProcedureAssetGraphNode* UProcedureAssetGraph::FindNode(const FGuid& InId) const
{
    for (UEdGraphNode* Node : Nodes)
    {
        UProcedureAssetGraphNode* ProcedureNode = Cast<UProcedureAssetGraphNode>(Node);
        if (ProcedureNode && ProcedureNode->NodeGuid == InId)
        {
            return ProcedureNode;
        }
    }
    return nullptr;
}

void UProcedureAssetGraph::Rebuild()
{
    Nodes.Reset();
    if (!Asset)
    {
        return;
    }
    const auto AddVisual = [this](const FGuid& Id, UProcedureBase* Definition, FVector2D Position)
    {
        UProcedureAssetGraphNode* Node = NewObject<UProcedureAssetGraphNode>(this, NAME_None, RF_Transient | RF_Transactional);
        Node->NodeGuid = Id;
        Node->Procedure = Definition;
        Node->bStart = Definition == nullptr;
        Node->NodePosX = FMath::RoundToInt(Position.X);
        Node->NodePosY = FMath::RoundToInt(Position.Y);
        Node->AllocateDefaultPins();
        AddNode(Node, false, false);
    };
    AddVisual(Asset->StartId, nullptr, Asset->StartPosition);
    for (UProcedureBase* Definition : Asset->Nodes)
    {
        if (Definition)
        {
            AddVisual(Definition->NodeId, Definition, Definition->GraphPosition);
        }
    }
    for (const FProcedureLink& Link : Asset->Links)
    {
        UProcedureAssetGraphNode* From = FindNode(Link.From);
        UProcedureAssetGraphNode* To = FindNode(Link.To);
        UEdGraphPin* Output = From ? From->FindPin(Link.Output, EGPD_Output) : nullptr;
        UEdGraphPin* Input = To ? To->FindPin(TEXT("In"), EGPD_Input) : nullptr;
        if (Output && Input)
        {
            Output->MakeLinkTo(Input);
        }
    }
}
