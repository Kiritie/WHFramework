#include "Voxel/Task/VoxelTerrainDependencyPriority.h"

void FVoxelTerrainDependencyPriority::Register(const FGuid& InNode, const FVoxelTerrainServicePriority& InOwnPriority)
{
    check(InNode.IsValid());
    if (Nodes.Contains(InNode))
    {
        SetPriority(InNode, InOwnPriority);
        return;
    }
    FNode& Node = Nodes.Add(InNode);
    Node.Own = InOwnPriority;
    Node.Effective = InOwnPriority;
    Dirty(InNode);
}

void FVoxelTerrainDependencyPriority::Dirty(const FGuid& InNode)
{
    if (Nodes.Contains(InNode) && !Queued.Contains(InNode))
    {
        Queued.Add(InNode);
        Queue.Add(InNode);
    }
}

bool FVoxelTerrainDependencyPriority::Reaches(const FGuid& InStart, const FGuid& InTarget) const
{
    TArray<FGuid> Stack{InStart};
    TSet<FGuid> Seen;
    while (!Stack.IsEmpty())
    {
        const FGuid Current = Stack.Pop(EAllowShrinking::No);
        if (Current == InTarget)
        {
            return true;
        }
        if (Seen.Contains(Current))
        {
            continue;
        }
        Seen.Add(Current);
        if (const FNode* Node = Nodes.Find(Current))
        {
            for (const FGuid& Dependency : Node->Dependencies)
            {
                Stack.Add(Dependency);
            }
        }
    }
    return false;
}

bool FVoxelTerrainDependencyPriority::SetDependencies(const FGuid& InNode, const TSet<FGuid>& InDependencies, FString& OutError)
{
    FNode* Node = Nodes.Find(InNode);
    if (!Node)
    {
        OutError = TEXT("依赖消费者尚未注册");
        return false;
    }
    if (Node->Dependencies.Num() == InDependencies.Num())
    {
        bool bSame = true;
        for (const FGuid& Dependency : InDependencies) bSame &= Node->Dependencies.Contains(Dependency);
        if (bSame)
        {
            OutError.Reset();
            return true;
        }
    }
    for (const FGuid& Dependency : InDependencies)
    {
        if (!Nodes.Contains(Dependency) || Dependency == InNode ||
            (!Node->Dependencies.Contains(Dependency) && Reaches(Dependency, InNode)))
        {
            OutError = TEXT("依赖不存在或形成循环");
            return false;
        }
    }
    const TSet<FGuid> Previous = Node->Dependencies;
    for (const FGuid& Dependency : Previous)
    {
        if (!InDependencies.Contains(Dependency))
        {
            Nodes.FindChecked(Dependency).Consumers.Remove(InNode);
            Dirty(Dependency);
        }
    }
    Node->Dependencies = InDependencies;
    for (const FGuid& Dependency : InDependencies)
    {
        if (!Previous.Contains(Dependency))
        {
            Nodes.FindChecked(Dependency).Consumers.Add(InNode);
            Dirty(Dependency);
        }
    }
    OutError.Reset();
    return true;
}

void FVoxelTerrainDependencyPriority::SetPriority(const FGuid& InNode, const FVoxelTerrainServicePriority& InPriority)
{
    FNode* Node = Nodes.Find(InNode);
    check(Node);
    if (Node->Own != InPriority)
    {
        Node->Own = InPriority;
        Dirty(InNode);
    }
}

void FVoxelTerrainDependencyPriority::Remove(const FGuid& InNode)
{
    FNode Removed;
    if (!Nodes.RemoveAndCopyValue(InNode, Removed))
    {
        return;
    }
    for (const FGuid& Dependency : Removed.Dependencies)
    {
        if (FNode* Node = Nodes.Find(Dependency))
        {
            Node->Consumers.Remove(InNode);
            Dirty(Dependency);
        }
    }
    for (const FGuid& Consumer : Removed.Consumers)
    {
        if (FNode* Node = Nodes.Find(Consumer))
        {
            Node->Dependencies.Remove(InNode);
        }
    }
    Queued.Remove(InNode);
}

int32 FVoxelTerrainDependencyPriority::Advance(const int32 InMaximumNodes,
    TFunctionRef<void(const FGuid&, const FVoxelTerrainServicePriority&)> InOnChanged)
{
    int32 Processed = 0;
    while (Queue.IsValidIndex(Cursor) && Processed < FMath::Max(0, InMaximumNodes))
    {
        const FGuid Key = Queue[Cursor++];
        Queued.Remove(Key);
        FNode* Node = Nodes.Find(Key);
        ++Processed;
        if (!Node)
        {
            continue;
        }
        FVoxelTerrainServicePriority Effective = Node->Own;
        for (const FGuid& Consumer : Node->Consumers)
        {
            if (const FNode* Parent = Nodes.Find(Consumer))
            {
                Effective = FVoxelTerrainWorkSelection::Promote(Effective, Parent->Effective);
            }
        }
        if (Node->Effective != Effective)
        {
            Node->Effective = Effective;
            const TSet<FGuid> Dependencies = Node->Dependencies;
            InOnChanged(Key, Effective);
            for (const FGuid& Dependency : Dependencies)
            {
                Dirty(Dependency);
            }
        }
    }
    if (Cursor == Queue.Num())
    {
        Queue.Reset();
        Cursor = 0;
    }
    return Processed;
}

const FVoxelTerrainServicePriority* FVoxelTerrainDependencyPriority::Find(const FGuid& InNode) const
{
    const FNode* Node = Nodes.Find(InNode);
    return Node ? &Node->Effective : nullptr;
}

bool FVoxelTerrainDependencyPriority::IsPending() const
{
    return Cursor < Queue.Num();
}

void FVoxelTerrainDependencyPriority::Reset()
{
    Nodes.Reset();
    Queue.Reset();
    Queued.Reset();
    Cursor = 0;
}
