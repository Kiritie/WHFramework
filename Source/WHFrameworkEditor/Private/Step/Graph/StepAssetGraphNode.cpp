#include "Step/Graph/StepAssetGraphNode.h"

#include "Step/Base/StepAsset.h"
#include "Step/Base/StepBase.h"
#include "Step/Graph/StepAssetGraph.h"
#include "Step/Graph/StepGraphOperations.h"
#include "EdGraphUtilities.h"
#include "SGraphNodeDefault.h"

void UStepAssetGraphNode::AllocateDefaultPins()
{
	if(!IsStart())
	{
		CreatePin(EGPD_Input, TEXT("Step"), TEXT("In"));
	}
	CreatePin(EGPD_Output, TEXT("Step"), TEXT("Out"));
}

FText UStepAssetGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if(IsStart())
	{
		return NSLOCTEXT("StepGraph", "Start", "Start");
	}
	const UStepAssetGraph* Graph = CastChecked<UStepAssetGraph>(GetGraph());
	const FStepCompiledNode* Compiled = Graph->StepAsset->CompiledGraph.Nodes.FindByPredicate([this](const FStepCompiledNode& Node){ return Node.StepGUID == StableGUID; });
	const FString Label = Compiled ? Compiled->OrderLabel : TEXT("—");
	return FText::Format(NSLOCTEXT("StepGraph", "OrderedTitle", "[{0}] {1}"), FText::FromString(Label), Step->StepDisplayName);
}

FText UStepAssetGraphNode::GetTooltipText() const
{
	return Step ? Step->StepDescription : NSLOCTEXT("StepGraph", "StartTip", "Only steps reachable from Start participate in execution.");
}

FLinearColor UStepAssetGraphNode::GetNodeTitleColor() const
{
	return IsStart() ? FLinearColor(0.12f, 0.50f, 0.26f) : FLinearColor(0.14f, 0.29f, 0.48f);
}

bool UStepAssetGraphNode::CanUserDeleteNode() const
{
	return !IsStart();
}

bool UStepAssetGraphNode::CanDuplicateNode() const
{
	return !IsStart();
}

class SStepAssetGraphNode : public SGraphNodeDefault
{
public:
	SLATE_BEGIN_ARGS(SStepAssetGraphNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UStepAssetGraphNode* InNode)
	{
		SGraphNodeDefault::Construct(SGraphNodeDefault::FArguments().GraphNodeObj(InNode));
	}

	virtual void MoveTo(const FVector2f& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty = true) override
	{
		if(NodeFilter.Contains(SharedThis(this)))
		{
			return;
		}
		UStepAssetGraphNode* Node = CastChecked<UStepAssetGraphNode>(GraphNode);
		UStepAssetGraph* Graph = CastChecked<UStepAssetGraph>(Node->GetGraph());
		const FVector2D Position(FMath::RoundToInt(NewPosition.X), FMath::RoundToInt(NewPosition.Y));
		const FVector2D Previous = Node->IsStart() ? Graph->StepAsset->StartPosition : Node->Step->GraphPosition;
		if(bMarkDirty && Position != Previous)
		{
			if(!FStepGraphOperations::CanEdit(Graph->StepAsset))
			{
				return;
			}
			FStepGraphOperations::ModifyForEdit(Graph->StepAsset);
			if(Node->IsStart())
			{
				Graph->StepAsset->StartPosition = Position;
			}
			else
			{
				Node->Step->Modify();
				Node->Step->GraphPosition = Position;
			}
			FStepGraphOperations::Compile(Graph->StepAsset);
		}
		SGraphNodeDefault::MoveTo(NewPosition, NodeFilter, bMarkDirty);
	}
};

class FStepAssetGraphNodeFactory : public FGraphPanelNodeFactory
{
public:
	virtual TSharedPtr<SGraphNode> CreateNode(UEdGraphNode* Node) const override
	{
		if(UStepAssetGraphNode* StepNode = Cast<UStepAssetGraphNode>(Node))
		{
			return SNew(SStepAssetGraphNode, StepNode);
		}
		return nullptr;
	}
};

TSharedRef<FGraphPanelNodeFactory> CreateStepGraphNodeFactory()
{
	return MakeShared<FStepAssetGraphNodeFactory>();
}
