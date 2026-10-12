#include "Step/Graph/StepAssetGraph.h"

#include "Step/Base/StepAsset.h"
#include "Step/Base/StepBase.h"
#include "Step/Graph/StepAssetGraphNode.h"

UStepAssetGraphNode* UStepAssetGraph::FindNode(const FString& InGUID) const
{
	for(UEdGraphNode* Node : Nodes)
	{
		UStepAssetGraphNode* StepNode = Cast<UStepAssetGraphNode>(Node);
		if(StepNode && StepNode->StableGUID == InGUID)
		{
			return StepNode;
		}
	}
	return nullptr;
}

void UStepAssetGraph::Rebuild()
{
	Nodes.Reset();
	Errors.Reset();
	if(!StepAsset)
	{
		return;
	}
	FStepCompiledGraph Preview;
	StepAsset->BuildCompiledGraph(Preview, Errors);
	const auto Add = [this](UStepBase* Step, const FString& GUID, FVector2D Position)
	{
		UStepAssetGraphNode* Node = NewObject<UStepAssetGraphNode>(this, NAME_None, RF_Transient | RF_Transactional);
		Node->Step = Step;
		Node->StableGUID = GUID;
		FGuid::Parse(GUID, Node->NodeGuid);
		Node->NodePosX = FMath::RoundToInt(Position.X);
		Node->NodePosY = FMath::RoundToInt(Position.Y);
		Node->AllocateDefaultPins();
		AddNode(Node, false, false);
	};
	Add(nullptr, StepAsset->StartGUID, StepAsset->StartPosition);
	for(UStepBase* Step : StepAsset->Steps)
	{
		if(Step)
		{
			Step->SetFlags(RF_Transactional);
			Add(Step, Step->StepGUID, Step->GraphPosition);
		}
	}
	for(const FStepGraphLink& Link : StepAsset->Links)
	{
		UStepAssetGraphNode* Parent = FindNode(Link.ParentGUID);
		UStepAssetGraphNode* Child = FindNode(Link.ChildGUID);
		UEdGraphPin* Output = Parent ? Parent->FindPin(TEXT("Out")) : nullptr;
		UEdGraphPin* Input = Child ? Child->FindPin(TEXT("In")) : nullptr;
		if(Output && Input)
		{
			Output->MakeLinkTo(Input);
		}
	}
}
