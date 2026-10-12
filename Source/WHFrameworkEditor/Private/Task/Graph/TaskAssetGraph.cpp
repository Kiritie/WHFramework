#include "Task/Graph/TaskAssetGraph.h"

#include "Task/Graph/TaskAssetGraphNode.h"
#include "Task/Base/TaskAsset.h"
#include "Task/Base/TaskBase.h"
#include "Misc/SecureHash.h"

UTaskAssetGraphNode* UTaskAssetGraph::FindNode(const FString& StableGUID) const
{
	for(UEdGraphNode* Node : Nodes)
	{
		UTaskAssetGraphNode* TaskNode = Cast<UTaskAssetGraphNode>(Node);
		if(TaskNode && TaskNode->GetStableGUID() == StableGUID)
		{
			return TaskNode;
		}
	}
	return nullptr;
}

void UTaskAssetGraph::RebuildFromAsset()
{
	Nodes.Reset();
	if(!TaskAsset)
	{
		return;
	}
	const bool bRuntime = TaskAsset->SourceObject != nullptr;
	const UTaskAsset* Source = bRuntime ? CastChecked<UTaskAsset>(TaskAsset->SourceObject) : TaskAsset;
	UTaskAssetGraphNode* StartNode = nullptr;
	if(Source->StartGUID.IsValid())
	{
		StartNode = NewObject<UTaskAssetGraphNode>(this, NAME_None, RF_Transactional);
		StartNode->bStartNode = true;
		StartNode->NodeGuid = Source->StartGUID;
		StartNode->NodePosX = FMath::RoundToInt(Source->StartPosition.X);
		StartNode->NodePosY = FMath::RoundToInt(Source->StartPosition.Y);
		StartNode->AllocateDefaultPins();
		AddNode(StartNode, false, false);
	}
	TArray<UTaskBase*> Tasks = TaskAsset->AuthoredTasks;
	if(bRuntime)
	{
		Tasks.Reset();
		TFunction<void(UTaskBase*)> Collect = [&](UTaskBase* Task)
		{
			Tasks.Add(Task);
			for(UTaskBase* Child : Task->SubTasks)
			{
				Collect(Child);
			}
		};
		for(UTaskBase* Root : TaskAsset->RootTasks)
		{
			Collect(Root);
		}
	}
	TMap<UTaskBase*, UTaskAssetGraphNode*> TaskNodes;
	for(UTaskBase* Task : Tasks)
	{
		if(!Task)
		{
			continue;
		}
		UTaskAssetGraphNode* Node = NewObject<UTaskAssetGraphNode>(this, NAME_None, RF_Transactional);
		Node->Task = Task;
		if(!FGuid::Parse(Task->TaskGUID, Node->NodeGuid))
		{
			FGuid::ParseExact(FMD5::HashAnsiString(*Task->TaskGUID), EGuidFormats::Digits, Node->NodeGuid);
		}
		Node->NodePosX = FMath::RoundToInt(Task->GraphPosition.X);
		Node->NodePosY = FMath::RoundToInt(Task->GraphPosition.Y);
		Node->AllocateDefaultPins();
		AddNode(Node, false, false);
		TaskNodes.Add(Task, Node);
	}
	auto Connect = [&](UTaskAssetGraphNode* Parent, UTaskBase* Child)
	{
		UTaskAssetGraphNode* const* ChildNode = TaskNodes.Find(Child);
		if(Parent && ChildNode)
		{
			UEdGraphPin* Output = Parent->FindPin(TEXT("Out"));
			UEdGraphPin* Input = (*ChildNode)->FindPin(TEXT("In"));
			if(Output && Input)
			{
				Output->MakeLinkTo(Input);
			}
		}
	};
	for(UTaskBase* Root : bRuntime ? TaskAsset->RootTasks : TaskAsset->StartTasks)
	{
		Connect(StartNode, Root);
	}
	for(const TPair<UTaskBase*, UTaskAssetGraphNode*>& Pair : TaskNodes)
	{
		for(UTaskBase* Child : bRuntime ? Pair.Key->SubTasks : Pair.Key->GraphChildren)
		{
			Connect(Pair.Value, Child);
		}
	}
}
