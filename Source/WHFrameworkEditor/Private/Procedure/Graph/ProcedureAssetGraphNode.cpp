#include "Procedure/Graph/ProcedureAssetGraphNode.h"

#include "EdGraphUtilities.h"
#include "SGraphNodeDefault.h"
#include "Procedure/Base/ProcedureAsset.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Procedure/Graph/ProcedureAssetGraph.h"
#include "Procedure/Graph/ProcedureGraphOperations.h"

void UProcedureAssetGraphNode::AllocateDefaultPins()
{
    if (bStart)
    {
        CreatePin(EGPD_Output, TEXT("Procedure"), TEXT("Out"));
        return;
    }
    CreatePin(EGPD_Input, TEXT("Procedure"), TEXT("In"));
    CreatePin(EGPD_Output, TEXT("Procedure"), TEXT("Out"));
}

FText UProcedureAssetGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    return bStart ? NSLOCTEXT("ProcedureGraph", "Start", "Start（开始）") :
        (Procedure ? Procedure->ProcedureDisplayName : NSLOCTEXT("ProcedureGraph", "Missing", "无效流程"));
}

FText UProcedureAssetGraphNode::GetTooltipText() const
{
    return bStart ? NSLOCTEXT("ProcedureGraph", "StartTip", "仅从此节点沿连线可达的流程进入运行结构") :
        (Procedure ? Procedure->ProcedureDescription : FText::GetEmpty());
}

FLinearColor UProcedureAssetGraphNode::GetNodeTitleColor() const
{
    if (bStart)
    {
        return FLinearColor(0.1f, 0.5f, 0.2f);
    }
    const UProcedureAssetGraph* Graph = Cast<UProcedureAssetGraph>(GetGraph());
    return Graph && Graph->Asset && Graph->Asset->CompiledNodes.Contains(Procedure) ?
        FLinearColor(0.12f, 0.3f, 0.55f) : FLinearColor(0.25f, 0.25f, 0.25f);
}

bool UProcedureAssetGraphNode::CanUserDeleteNode() const
{
    return !bStart;
}

bool UProcedureAssetGraphNode::CanDuplicateNode() const
{
    return !bStart;
}

class SProcedureAssetGraphNode : public SGraphNodeDefault
{
public:
    SLATE_BEGIN_ARGS(SProcedureAssetGraphNode) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, UProcedureAssetGraphNode* InNode)
    {
        SGraphNodeDefault::Construct(SGraphNodeDefault::FArguments().GraphNodeObj(InNode));
    }

    virtual void MoveTo(const FVector2f& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty = true) override
    {
        UProcedureAssetGraphNode* Node = CastChecked<UProcedureAssetGraphNode>(GraphNode);
        UProcedureAssetGraph* Graph = CastChecked<UProcedureAssetGraph>(Node->GetGraph());
        if (NodeFilter.Contains(SharedThis(this)))
        {
            return;
        }
        const FVector2D Position(FMath::RoundToInt(NewPosition.X), FMath::RoundToInt(NewPosition.Y));
        if (bMarkDirty && (Node->NodePosX != Position.X || Node->NodePosY != Position.Y))
        {
            if (!FProcedureGraphOperations::CanEdit(Graph->Asset))
            {
                return;
            }
            Graph->Modify();
            Node->Modify();
            Graph->Asset->Modify();
            if (Node->bStart)
            {
                Graph->Asset->StartPosition = Position;
            }
            else if (Node->Procedure)
            {
                Node->Procedure->Modify();
                Node->Procedure->GraphPosition = Position;
            }
        }
        SGraphNodeDefault::MoveTo(NewPosition, NodeFilter, bMarkDirty);
    }
};

class FProcedureAssetGraphNodeFactory : public FGraphPanelNodeFactory
{
public:
    virtual TSharedPtr<SGraphNode> CreateNode(UEdGraphNode* InNode) const override
    {
        if (UProcedureAssetGraphNode* Node = Cast<UProcedureAssetGraphNode>(InNode))
        {
            return SNew(SProcedureAssetGraphNode, Node);
        }
        return nullptr;
    }
};

TSharedRef<FGraphPanelNodeFactory> CreateProcedureGraphNodeFactory()
{
    return MakeShared<FProcedureAssetGraphNodeFactory>();
}
