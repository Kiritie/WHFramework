#include "Voxel/Generation/Hydrology/VoxelHydrologyProfile.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

bool VoxelHydrology::SolveRiverProfile(TConstArrayView<FVoxelRiverProfileNode> InNodes,
    TConstArrayView<FVoxelRiverProfileEdge> InEdges, TArray<int32>& OutWaterZ,
    int32& OutConflictNode, FString& OutError, const TAtomic<bool>* InCancel,
    int32* OutConflictLowerWaterZ)
{
    OutConflictNode = INDEX_NONE;
    if (OutConflictLowerWaterZ) *OutConflictLowerWaterZ = MIN_int32;
    auto Canceled = [&]()
    {
        if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return true; }
        return false;
    };
    if (Canceled()) return false;
    const int32 Count = InNodes.Num();
    if (Count == MAX_int32)
    {
        OutError = TEXT("Oversized: profile node offsets exceed native range");
        return false;
    }
    FVoxelGenerationWorkspace Workspace;
    TArray<int32> Lower, Upper, Indegree, Offsets, Next, Targets, Order;
    if (!Workspace.ReserveArray(Lower, Count, OutError) ||
        !Workspace.ReserveArray(Upper, Count, OutError) ||
        !Workspace.ReserveArray(Indegree, Count, OutError) ||
        !Workspace.ReserveArray(Offsets, Count + 1, OutError) ||
        !Workspace.ReserveArray(Next, Count, OutError) ||
        !Workspace.ReserveArray(Targets, InEdges.Num(), OutError) ||
        !Workspace.ReserveArray(Order, Count, OutError)) return false;
    Lower.SetNumUninitialized(Count);
    Upper.SetNumUninitialized(Count);
    Indegree.Init(0, Count);
    Offsets.Init(0, Count + 1);
    Next.SetNumUninitialized(Count);
    Targets.SetNumUninitialized(InEdges.Num());
    for (int32 Index = 0; Index < Count; ++Index)
    {
        if ((Index & 255) == 0 && Canceled()) return false;
        const auto& Node = InNodes[Index];
        if (Node.LowerWaterZ < -32768 || Node.UpperWaterZ > 32766 ||
            (Node.bFixed && (Node.FixedWaterZ < -32768 || Node.FixedWaterZ > 32766)))
        {
            OutConflictNode = Index;
            OutError = TEXT("Invalid river profile water plane outside section domain");
            return false;
        }
        Lower[Index] = Node.bFixed ? FMath::Max(Node.LowerWaterZ, Node.FixedWaterZ) : Node.LowerWaterZ;
        Upper[Index] = Node.bFixed ? FMath::Min(Node.UpperWaterZ, Node.FixedWaterZ) : Node.UpperWaterZ;
    }
    for (int32 Index = 0; Index < InEdges.Num(); ++Index)
    {
        if ((Index & 255) == 0 && Canceled()) return false;
        const auto& Edge = InEdges[Index];
        if (Edge.Upstream < 0 || Edge.Upstream >= Count || Edge.Downstream < 0 ||
            Edge.Downstream >= Count || Edge.Upstream == Edge.Downstream)
        {
            OutError = TEXT("Invalid river profile edge");
            return false;
        }
        ++Offsets[Edge.Upstream + 1];
        ++Indegree[Edge.Downstream];
    }
    for (int32 Index = 0; Index < Count; ++Index)
    {
        if ((Index & 255) == 0 && Canceled()) return false;
        Offsets[Index + 1] += Offsets[Index];
        Next[Index] = Offsets[Index];
        if (Indegree[Index] == 0) Order.Add(Index);
    }
    for (int32 Index = 0; Index < InEdges.Num(); ++Index)
    {
        if ((Index & 255) == 0 && Canceled()) return false;
        const auto& Edge = InEdges[Index];
        Targets[Next[Edge.Upstream]++] = Edge.Downstream;
    }
    for (int32 QueueIndex = 0; QueueIndex < Order.Num(); ++QueueIndex)
    {
        if ((QueueIndex & 255) == 0 && Canceled()) return false;
        const int32 Up = Order[QueueIndex];
        for (int32 Index = Offsets[Up]; Index < Offsets[Up + 1]; ++Index)
        {
            if ((Index & 255) == 0 && Canceled()) return false;
            const int32 Down = Targets[Index];
            Upper[Down] = FMath::Min(Upper[Down], Upper[Up]);
            if (--Indegree[Down] == 0) Order.Add(Down);
        }
    }
    if (Order.Num() != Count)
    {
        OutError = TEXT("Cyclic river profile topology requires canonical graph repair");
        return false;
    }
    for (int32 QueueIndex = Order.Num() - 1; QueueIndex >= 0; --QueueIndex)
    {
        if ((QueueIndex & 255) == 0 && Canceled()) return false;
        const int32 Up = Order[QueueIndex];
        for (int32 Index = Offsets[Up]; Index < Offsets[Up + 1]; ++Index)
        {
            if ((Index & 255) == 0 && Canceled()) return false;
            Lower[Up] = FMath::Max(Lower[Up], Lower[Targets[Index]]);
        }
    }
    for (int32 Index = 0; Index < Count; ++Index)
    {
        if ((Index & 255) == 0 && Canceled()) return false;
        if (Lower[Index] > Upper[Index])
        {
            OutConflictNode = Index;
            if (OutConflictLowerWaterZ) *OutConflictLowerWaterZ = Lower[Index];
            OutError = TEXT("Infeasible river profile: fixed receiver or cut limit exceeds final-corridor cap");
            return false;
        }
    }
    if (Canceled() || !VoxelGenerationAllocation::RetainArray(OutWaterZ, Count, OutError)) return false;
    OutWaterZ.SetNumUninitialized(Count);
    for (int32 Index = 0; Index < Count; ++Index) OutWaterZ[Index] = Upper[Index];
    OutError.Reset();
    return true;
}
