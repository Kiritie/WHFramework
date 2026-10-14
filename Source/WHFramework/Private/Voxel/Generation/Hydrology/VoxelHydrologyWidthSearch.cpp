#include "Voxel/Generation/Hydrology/VoxelHydrologyWidthSearch.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

namespace
{
    int32 WidthSearchCapacity(int32 Old) { return int32(FMath::Min<int64>(MAX_int32,FMath::Max<int64>(16,int64(Old)*2))); }
}
bool FVoxelHydrologyWidthSearch::AppendNode(const FVoxelHydrologyWidthGraphNode& Node, int32& Index, FString& Error, int32 IncomingEdge)
{
    if (Nodes.Num() == MAX_int32) { Error = TEXT("Oversized: width search nodes"); return false; }
    if (Nodes.Num() == Nodes.Max() && !VoxelGenerationAllocation::RetainArray(Nodes,WidthSearchCapacity(Nodes.Max()),Error)) return false;
    if (DiscoveryEdge.Num() == DiscoveryEdge.Max() && !VoxelGenerationAllocation::RetainArray(DiscoveryEdge,WidthSearchCapacity(DiscoveryEdge.Max()),Error)) return false;
    if (ByCell.Num() == MapCapacity)
    {
        const int32 Capacity = WidthSearchCapacity(MapCapacity);
        if (!VoxelGenerationAllocation::RetainMap(ByCell,Capacity,Error)) return false;
        MapCapacity = Capacity;
    }
    Index = Nodes.Add(Node); DiscoveryEdge.Add(IncomingEdge); ByCell.Add(Node.Cell,Index); return true;
}
EVoxelHydrologySharedSearchStep FVoxelHydrologyWidthSearch::Advance(FReadNode ReadNode, FReadEdge ReadEdge,
    int32 WorkBudget, FString& OutError, const TAtomic<bool>* Cancel)
{
    OutError.Reset();
    auto Failure = [&]() { return OutError == TEXT("Canceled") ? EVoxelHydrologySharedSearchStep::Canceled : OutError.StartsWith(TEXT("CapacityBlocked")) ? EVoxelHydrologySharedSearchStep::CapacityBlocked :
        OutError.StartsWith(TEXT("Yielded")) ? EVoxelHydrologySharedSearchStep::Yielded : EVoxelHydrologySharedSearchStep::Failed; };
    if (Cancel && Cancel->Load()) return EVoxelHydrologySharedSearchStep::Canceled;
    if (!bInitialized) { OutError = TEXT("Width search not initialized"); return Failure(); }
    if (bReady) return EVoxelHydrologySharedSearchStep::Ready;
    if (WorkBudget <= 0) return EVoxelHydrologySharedSearchStep::Yielded;
    if (!bSeedRead)
    {
        FVoxelHydrologyWidthGraphNode Node;
        const auto Status = ReadNode(Seed,Node,OutError);
        if (Status == EVoxelHydrologyClosureRead::Pending) return OutError == TEXT("Canceled") ? EVoxelHydrologySharedSearchStep::Canceled : OutError.StartsWith(TEXT("Yielded")) ? EVoxelHydrologySharedSearchStep::Yielded : OutError.StartsWith(TEXT("CapacityBlocked")) ? EVoxelHydrologySharedSearchStep::CapacityBlocked : EVoxelHydrologySharedSearchStep::NeedInput;
        if (Status != EVoxelHydrologyClosureRead::Ready) return Failure();
        Node.Cell = Seed; Threshold = Node.Interval.Upper;
        if (Node.Interval.Lower > Node.Interval.Upper)
        { bSeedRead = bClosed = bReady = true; return EVoxelHydrologySharedSearchStep::Ready; }
        int32 Index;
        if (!AppendNode(Node,Index,OutError)) return Failure();
        bSeedRead = true; --WorkBudget;
    }
    while (!bClosed && WorkBudget > 0)
    {
        --WorkBudget;
        if (Cancel && Cancel->Load()) return EVoxelHydrologySharedSearchStep::Canceled;

        if (WitnessNode != INDEX_NONE)
        {
            if (WitnessNode == 0)
            { Threshold = FMath::Min(Threshold,WitnessWater); WitnessNode = INDEX_NONE; continue; }
            const int32 Incoming = DiscoveryEdge[WitnessNode];
            if (Incoming == INDEX_NONE) { WitnessNode = INDEX_NONE; continue; }
            const auto& WitnessEdge = Edges[Incoming];
            if (WitnessWater < WitnessEdge.DownLower || WitnessWater > WitnessEdge.B)
            { WitnessNode = INDEX_NONE; continue; }
            const int64 UpWater = WitnessEdge.bHasRaise ? FMath::Max(WitnessWater,WitnessEdge.A) : WitnessWater;
            const auto& Up = Nodes[WitnessEdge.Upstream];
            if (UpWater < Up.Interval.Lower || UpWater > Up.Interval.Upper)
            { WitnessNode = INDEX_NONE; continue; }
            WitnessWater = UpWater; WitnessNode = WitnessEdge.Upstream; continue;
        }
        if (Cursor >= Nodes.Num()) { bClosed = true; break; }
        const auto Node = Nodes[Cursor];
        if (Node.bFixed || Node.Interval.Lower > Threshold || Face == 4) { ++Cursor; Face = 0; continue; }
        FVoxelHydrologyWidthSearchEdge Input;
        auto Status = ReadEdge(Node.Cell,Face,Input,OutError);
        if (Status == EVoxelHydrologyClosureRead::Pending) return OutError == TEXT("Canceled") ? EVoxelHydrologySharedSearchStep::Canceled : OutError.StartsWith(TEXT("Yielded")) ? EVoxelHydrologySharedSearchStep::Yielded : OutError.StartsWith(TEXT("CapacityBlocked")) ? EVoxelHydrologySharedSearchStep::CapacityBlocked : EVoxelHydrologySharedSearchStep::NeedInput;
        if (Status != EVoxelHydrologyClosureRead::Ready) return Failure();
        auto Edge = Input.Transfer;
        if (!Edge.bFeasible || (Edge.bHasRaise && Edge.A > Threshold) || Edge.DownLower > Threshold)
        { ++Face; continue; }
        FVoxelHydrologyWidthGraphNode Down;
        Status = ReadNode(Input.Downstream,Down,OutError);
        if (Status == EVoxelHydrologyClosureRead::Pending) return OutError == TEXT("Canceled") ? EVoxelHydrologySharedSearchStep::Canceled : OutError.StartsWith(TEXT("Yielded")) ? EVoxelHydrologySharedSearchStep::Yielded : OutError.StartsWith(TEXT("CapacityBlocked")) ? EVoxelHydrologySharedSearchStep::CapacityBlocked : EVoxelHydrologySharedSearchStep::NeedInput;
        if (Status != EVoxelHydrologyClosureRead::Ready) return Failure();
        Down.Cell = Input.Downstream;
        if (Edge.DownLower != Down.Interval.Lower)
        { OutError = TEXT("Width transfer downstream lower does not match node"); return Failure(); }
        if (Down.Interval.Lower > Down.Interval.Upper || Down.Interval.Lower > Threshold ||
            (Down.bFixed && Down.FixedH > Threshold)) { ++Face; continue; }
        if (Edges.Num() == MAX_int32) { OutError = TEXT("Oversized: width search edges"); return Failure(); }
        if (Edges.Num() == Edges.Max() && !VoxelGenerationAllocation::RetainArray(Edges,WidthSearchCapacity(Edges.Max()),OutError)) return Failure();
        int32 DownIndex;
        if (const int32* Existing = ByCell.Find(Down.Cell)) DownIndex = *Existing;
        else if (!AppendNode(Down,DownIndex,OutError,Edges.Num())) return Failure();
        Edge.Upstream = Cursor; Edge.Downstream = DownIndex;
        Edges.Add(Edge); ++Face;
        if (Down.bFixed && Down.FixedH >= Down.Interval.Lower && Down.FixedH <= Down.Interval.Upper &&
            Down.FixedH >= Edge.DownLower && Down.FixedH <= Edge.B)
        {
            const int64 UpWater = Edge.bHasRaise ? FMath::Max(Down.FixedH,Edge.A) : Down.FixedH;
            if (UpWater >= Node.Interval.Lower && UpWater <= Node.Interval.Upper)
            { WitnessNode = Cursor; WitnessWater = UpWater; }
        }
    }
    if (!bClosed || WorkBudget <= 0) return EVoxelHydrologySharedSearchStep::Yielded;
    const auto Step = Solver.Advance(Nodes,Edges,true,WorkBudget,Cancel && Cancel->Load());
    if (Step == EVoxelHydrologyWidthStep::Canceled) return EVoxelHydrologySharedSearchStep::Canceled;
    if (Step == EVoxelHydrologyWidthStep::CapacityBlocked) { OutError = TEXT("CapacityBlocked: width solver"); return EVoxelHydrologySharedSearchStep::CapacityBlocked; }
    if (Step == EVoxelHydrologyWidthStep::Pending) return EVoxelHydrologySharedSearchStep::Yielded;
    if (Step != EVoxelHydrologyWidthStep::Ready) { OutError = TEXT("Invalid certified width graph"); return Failure(); }
    SeedLabel = Solver.GetLabels()[0];
    if (SeedLabel.bEligible && SeedLabel.W > Threshold)
    { OutError = TEXT("Width search returned water outside seed interval"); return Failure(); }
    bReady = true; return EVoxelHydrologySharedSearchStep::Ready;
}
uint64 FVoxelHydrologyWidthSearch::GetAllocatedBytes() const
{ return VoxelGenerationAllocation::AllocatedBytes(Nodes)+VoxelGenerationAllocation::AllocatedBytes(Edges)+VoxelGenerationAllocation::AllocatedBytes(DiscoveryEdge)+VoxelGenerationAllocation::AllocatedBytes(ByCell)+Solver.GetAllocatedBytes(); }
void FVoxelHydrologyWidthSearch::FreeBacking()
{
    VoxelGenerationAllocation::FreeRetainedArray(Nodes);
    VoxelGenerationAllocation::FreeRetainedArray(Edges);
    VoxelGenerationAllocation::FreeRetainedArray(DiscoveryEdge);
    VoxelGenerationAllocation::FreeRetainedMap(ByCell);
    Solver.FreeBacking();
    bInitialized = bSeedRead = bClosed = bReady = false;
    Cursor = Face = MapCapacity = 0; WitnessNode = INDEX_NONE; SeedLabel = FVoxelHydrologyWidthGraphLabel();
}
