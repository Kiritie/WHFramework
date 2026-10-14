#include "Voxel/Generation/Hydrology/VoxelHydrologyWidthRoutingQuery.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

namespace
{
    int32 WidthRoutingCapacity(int32 Old)
    { return int32(FMath::Min<int64>(MAX_int32, FMath::Max<int64>(16, int64(Old) * 2))); }

    EVoxelHydrologyClosureRead WidthRoutingReadStatus(EVoxelHydrologySharedSearchStep Step, FString& Error)
    {
        switch (Step)
        {
        case EVoxelHydrologySharedSearchStep::Ready: return EVoxelHydrologyClosureRead::Ready;
        case EVoxelHydrologySharedSearchStep::Yielded:
            Error = TEXT("Yielded: width routing certificate"); return EVoxelHydrologyClosureRead::Pending;
        case EVoxelHydrologySharedSearchStep::CapacityBlocked:
            if (!Error.StartsWith(TEXT("CapacityBlocked"))) Error = TEXT("CapacityBlocked: width routing certificate");
            return EVoxelHydrologyClosureRead::Pending;
        case EVoxelHydrologySharedSearchStep::Canceled:
            Error = TEXT("Canceled"); return EVoxelHydrologyClosureRead::Pending;
        case EVoxelHydrologySharedSearchStep::NeedInput: return EVoxelHydrologyClosureRead::Pending;
        default: return EVoxelHydrologyClosureRead::Failed;
        }
    }
}

bool FVoxelHydrologyWidthRoutingQuery::Initialize(const FVoxelHydrologyRoutingGrid& InGrid,
    const FVoxelRiverShape& InShape, int32 InMinimumGround, const FIntPoint& InWorldMin,
    const FIntPoint& InWorldMax, uint64 InRecipeIdentity, FString& OutError)
{
    OutError.Reset();
    if (bInitialized || InWorldMin.X > InWorldMax.X || InWorldMin.Y > InWorldMax.Y)
    { OutError = TEXT("Invalid width routing query initialization"); return false; }

    FVoxelHydrologySectionCertificate Validate;
    if (!Validate.Initialize(InWorldMin, InShape, InMinimumGround, InWorldMin, InWorldMax, OutError)) return false;
    const int64 K = int64(InShape.WaterHalfWidth) + InShape.BankWidth + InShape.ShoreWidth + 1;

    if (InGrid.GetSpacing() <= 4 * K)
    { OutError = TEXT("Routing spacing must exceed four junction half-lengths"); return false; }
    Grid = InGrid; Shape = InShape; MinimumGround = InMinimumGround;
    WorldMin = InWorldMin; WorldMax = InWorldMax; RecipeIdentity = InRecipeIdentity;
    EqualityHalfLength = int32(K); bInitialized = true; return true;
}

bool FVoxelHydrologyWidthRoutingQuery::AddNode(const FIntPoint& Position, int32& OutIndex, FString& Error)
{
    if (const int32* Existing = ByPosition.Find(Position)) { OutIndex = *Existing; return true; }
    if (Nodes.Num() == MAX_int32) { Error = TEXT("Oversized: width routing node cache"); return false; }
    if (Nodes.Num() == Nodes.Max() && !VoxelGenerationAllocation::RetainArray(Nodes,
        WidthRoutingCapacity(Nodes.Max()), Error)) return false;
    if (ByPosition.Num() == NodeMapCapacity)
    {
        const int32 Capacity = WidthRoutingCapacity(NodeMapCapacity);
        if (!VoxelGenerationAllocation::RetainMap(ByPosition, Capacity, Error)) return false;
        NodeMapCapacity = Capacity;
    }
    OutIndex = Nodes.AddDefaulted();
    Nodes[OutIndex].Certificate.Position = Position;
    Nodes[OutIndex].Certificate.ConnectorEnd = Position;
    Nodes[OutIndex].Certificate.bGridNode = Grid.IsGridNode(Position);
    ByPosition.Add(Position, OutIndex); return true;
}

EVoxelHydrologySharedSearchStep FVoxelHydrologyWidthRoutingQuery::EnsureNode(const FIntPoint& Position,
    FReadSample Read, int32& Budget, int32& OutIndex, FString& Error, const TAtomic<bool>* Cancel)
{
    if (Position.X < WorldMin.X || Position.X > WorldMax.X || Position.Y < WorldMin.Y || Position.Y > WorldMax.Y)
    { Error = TEXT("Width routing node outside physical world"); return EVoxelHydrologySharedSearchStep::Failed; }
    if (!AddNode(Position, OutIndex, Error))
        return Error.StartsWith(TEXT("Oversized")) ? EVoxelHydrologySharedSearchStep::Failed : EVoxelHydrologySharedSearchStep::CapacityBlocked;
    auto& Entry = Nodes[OutIndex];
    if (Entry.bReady) return EVoxelHydrologySharedSearchStep::Ready;
    if (Budget <= 0) return EVoxelHydrologySharedSearchStep::Yielded;
    if (!Entry.bStarted)
    {
        const bool Ok = Entry.Certificate.bGridNode ?
            Entry.Junction.Initialize(Position, Shape, MinimumGround, WorldMin, WorldMax, Error) :
            Entry.Section.Initialize(Position, Shape, MinimumGround, WorldMin, WorldMax, Error);
        if (!Ok) return EVoxelHydrologySharedSearchStep::Failed;
        Entry.bStarted = true;
    }
    const int32 CertificateBudget = Budget; Budget = 0;
    const auto Step = Entry.Certificate.bGridNode ? Entry.Junction.Advance(Read, CertificateBudget, Error, Cancel) :
        Entry.Section.Advance(Read, CertificateBudget, Error, Cancel);
    if (Step != EVoxelHydrologySharedSearchStep::Ready) return Step;
    Entry.Certificate.Interval = Entry.Certificate.bGridNode ? Entry.Junction.GetResult() : Entry.Section.GetResult();
    if (Entry.Certificate.bGridNode) Entry.Certificate.ConnectorEnd = Entry.Junction.GetConnectorEnd();


    if (!Entry.Certificate.bGridNode && Entry.Certificate.Interval.FixedWaterZ == MIN_int32)
        Entry.Certificate.Interval.bEligible = false;
    Entry.bReady = true; return EVoxelHydrologySharedSearchStep::Ready;
}

EVoxelHydrologySharedSearchStep FVoxelHydrologyWidthRoutingQuery::EnsurePort(const FIntPoint& Position,
    int32 Face, FReadSample Read, int32& Budget, FVoxelHydrologyWidthSearchEdge& Out,
    FString& Error, const TAtomic<bool>* Cancel)
{
    if (Face < 0 || Face >= 4) { Error = TEXT("Invalid width routing face"); return EVoxelHydrologySharedSearchStep::Failed; }
    int32 NodeIndex = INDEX_NONE;
    auto Step = EnsureNode(Position, Read, Budget, NodeIndex, Error, Cancel);
    if (Step != EVoxelHydrologySharedSearchStep::Ready) return Step;
    const auto Start = Nodes[NodeIndex].Certificate;
    if (!Start.bGridNode || !Start.Interval.bEligible || Start.Interval.FixedWaterZ != MIN_int32)
    { Out = FVoxelHydrologyWidthSearchEdge(); return EVoxelHydrologySharedSearchStep::Ready; }
    int32 PortIndex = Nodes[NodeIndex].Ports[Face];
    if (PortIndex == INDEX_NONE)
    {
        if (Ports.Num() == MAX_int32) { Error = TEXT("Oversized: width routing port cache"); return EVoxelHydrologySharedSearchStep::Failed; }
        if (Ports.Num() == Ports.Max() && !VoxelGenerationAllocation::RetainArray(Ports,
            WidthRoutingCapacity(Ports.Max()), Error)) return EVoxelHydrologySharedSearchStep::CapacityBlocked;
        PortIndex = Ports.AddDefaulted(); Nodes[NodeIndex].Ports[Face] = PortIndex;
        if (!Grid.TryNeighbor(Position, Face, Ports[PortIndex].End)) Ports[PortIndex].bReady = true;
    }
    if (Ports[PortIndex].bReady) { Out = Ports[PortIndex].Result; return EVoxelHydrologySharedSearchStep::Ready; }
    if (!Ports[PortIndex].bStarted)
    {
        FVoxelHydrologySectionInterval EndBlock;
        const FIntPoint End = Ports[PortIndex].End;
        const bool bGridEnd = Grid.IsGridNode(End);
        if (bGridEnd)
        {
            int32 EndIndex = INDEX_NONE;
            Step = EnsureNode(End, Read, Budget, EndIndex, Error, Cancel);
            if (Step != EVoxelHydrologySharedSearchStep::Ready) return Step;
            EndBlock = Nodes[EndIndex].Certificate.Interval;
        }
        if (Budget <= 0) return EVoxelHydrologySharedSearchStep::Yielded;
        auto& Port = Ports[PortIndex].Port;
        if (!Port.Initialize(Position, End, Shape, MinimumGround, WorldMin, WorldMax, Error))
            return EVoxelHydrologySharedSearchStep::Failed;
        const bool Ok = bGridEnd ? Port.SetEndpointEqualityBlocks(EqualityHalfLength, Start.Interval, EndBlock, Error) :
            Port.SetStartEndpointEqualityBlock(EqualityHalfLength, Start.Interval, Error);
        if (!Ok) return EVoxelHydrologySharedSearchStep::Failed;
        Ports[PortIndex].bStarted = true;
    }
    if (Budget <= 0) return EVoxelHydrologySharedSearchStep::Yielded;
    const int32 PortBudget = Budget; Budget = 0;
    Step = Ports[PortIndex].Port.Advance(Read, PortBudget, Error, Cancel);
    if (Step != EVoxelHydrologySharedSearchStep::Ready) return Step;
    auto& Entry = Ports[PortIndex];
    Entry.Result = Entry.Port.GetResult();


    if (Entry.Result.Transfer.bFeasible && !Grid.IsGridNode(Entry.Result.Downstream) &&
        (!Entry.Port.GetTerminal().bEligible || Entry.Port.GetTerminal().FixedWaterZ == MIN_int32))
        Entry.Result.Transfer.bFeasible = false;
    Entry.bReady = true; Out = Entry.Result; return EVoxelHydrologySharedSearchStep::Ready;
}

EVoxelHydrologySharedSearchStep FVoxelHydrologyWidthRoutingQuery::Advance(const FIntPoint& Seed,
    FReadSample ReadSample, int32 WorkBudget, FString& OutError, const TAtomic<bool>* Cancel)
{
    OutError.Reset();
    if (Cancel && Cancel->Load()) return EVoxelHydrologySharedSearchStep::Canceled;
    if (!bInitialized) { OutError = TEXT("Width routing query is not initialized"); return EVoxelHydrologySharedSearchStep::Failed; }
    if (Find(Seed)) return EVoxelHydrologySharedSearchStep::Ready;
    if (bSearchActive && Seed != ActiveSeed)
    { OutError = TEXT("Finish the pending width routing seed before requesting another"); return EVoxelHydrologySharedSearchStep::Failed; }
    if (WorkBudget <= 0) return EVoxelHydrologySharedSearchStep::Yielded;
    if (!bSearchActive) { ActiveSeed = Seed; Search.Initialize(Seed); bSearchActive = true; }
    int32 Remaining = WorkBudget;
    auto ReadNode = [&](const FIntPoint& Position, FVoxelHydrologyWidthGraphNode& Out, FString& Error)
    {
        int32 Index = INDEX_NONE;
        const auto Step = EnsureNode(Position, ReadSample, Remaining, Index, Error, Cancel);
        if (Step != EVoxelHydrologySharedSearchStep::Ready) return WidthRoutingReadStatus(Step, Error);
        const auto& Certificate = Nodes[Index].Certificate;
        Out.Cell = Position;


        Out.Interval = Certificate.Interval.bEligible ? FVoxelHydrologyWidthNodeInterval{
            Certificate.Interval.LowerWaterZ, Certificate.Interval.UpperWaterZ} : FVoxelHydrologyWidthNodeInterval{1, 0};
        Out.bFixed = Certificate.Interval.bEligible && Certificate.Interval.FixedWaterZ != MIN_int32;
        Out.FixedH = Out.bFixed ? Certificate.Interval.FixedWaterZ : 0;
        return EVoxelHydrologyClosureRead::Ready;
    };
    auto ReadEdge = [&](const FIntPoint& Position, int32 Face, FVoxelHydrologyWidthSearchEdge& Out, FString& Error)
    { return WidthRoutingReadStatus(EnsurePort(Position, Face, ReadSample, Remaining, Out, Error, Cancel), Error); };
    while (Remaining > 0)
    {
        const int32 Before = Remaining;


        const auto Step = Search.Advance(ReadNode, ReadEdge, 1, OutError, Cancel);
        if (Step == EVoxelHydrologySharedSearchStep::Ready)
        {
            if (!PublishSeed(OutError)) return EVoxelHydrologySharedSearchStep::Failed;
            Search.FreeBacking(); bSearchActive = false;
            return EVoxelHydrologySharedSearchStep::Ready;
        }
        if (Step != EVoxelHydrologySharedSearchStep::Yielded) return Step;
        if (Remaining == Before) --Remaining;
    }
    return EVoxelHydrologySharedSearchStep::Yielded;
}

bool FVoxelHydrologyWidthRoutingQuery::PublishSeed(FString& Error)
{
    const int32* Index = ByPosition.Find(ActiveSeed);
    if (!Index || !Nodes[*Index].bReady) { Error = TEXT("Missing width routing seed certificate"); return false; }
    auto& Entry = Nodes[*Index];
    const auto& Input = Search.GetSeedLabel();
    FVoxelHydrologyWidthRoutingLabel Label;
    Label.W = Input.W; Label.D = Input.D; Label.RecipeIdentity = RecipeIdentity;
    Label.bEligible = Input.bEligible; Label.bHasDistance = Input.bHasDistance;
    if (Input.bEligible && Input.Parent != INDEX_NONE)
    {
        const auto& SearchNodes = Search.GetNodes();
        const auto& SearchEdges = Search.GetEdges();
        if (!SearchNodes.IsValidIndex(Input.Parent) || !SearchEdges.IsValidIndex(Input.ParentEdge))
        { Error = TEXT("Invalid local width routing parent index"); return false; }
        Label.Parent = SearchNodes[Input.Parent].Cell;
        const auto& Edge = SearchEdges[Input.ParentEdge];
        if (Edge.Upstream != 0 || Edge.Downstream != Input.Parent)
        { Error = TEXT("Selected width routing edge does not belong to its seed"); return false; }
        Label.PathId = Edge.PathId; Label.FootprintVersion = Edge.FootprintVersion;
        for (int32 Face = 0; Face < 4; ++Face)
        {
            const int32 PortIndex = Entry.Ports[Face];
            if (PortIndex == INDEX_NONE) continue;
            const auto& Port = Ports[PortIndex];
            if (Port.bReady && Port.Result.Transfer.bFeasible && Port.Result.Downstream == Label.Parent &&
                Port.Result.Transfer.PathId == Label.PathId && Port.Result.Transfer.FootprintVersion == Label.FootprintVersion)
            { Label.ParentFace = Face; Label.bHasParent = true; break; }
        }
        if (!Label.bHasParent) { Error = TEXT("Missing exact selected width routing port"); return false; }
    }
    else if (Input.bEligible && Entry.Certificate.Interval.FixedWaterZ == MIN_int32)
    { Error = TEXT("Eligible width routing node has neither parent nor real root"); return false; }
    Entry.Label = Label; Entry.bLabelReady = true; return true;
}

const FVoxelHydrologyWidthRoutingLabel* FVoxelHydrologyWidthRoutingQuery::Find(const FIntPoint& Seed) const
{
    const int32* Index = ByPosition.Find(Seed);
    return Index && Nodes[*Index].bLabelReady ? &Nodes[*Index].Label : nullptr;
}
const FVoxelHydrologyWidthRoutingNode* FVoxelHydrologyWidthRoutingQuery::FindNode(const FIntPoint& Position) const
{
    const int32* Index = ByPosition.Find(Position);
    return Index && Nodes[*Index].bReady ? &Nodes[*Index].Certificate : nullptr;
}
int32 FVoxelHydrologyWidthRoutingQuery::SelectedPortIndex(const FIntPoint& Seed) const
{
    const int32* Index = ByPosition.Find(Seed);
    if (!Index) return INDEX_NONE;
    const auto& Entry = Nodes[*Index];
    return Entry.bLabelReady && Entry.Label.bEligible && Entry.Label.bHasParent ? Entry.Ports[Entry.Label.ParentFace] : INDEX_NONE;
}
const FVoxelHydrologyWidthPort* FVoxelHydrologyWidthRoutingQuery::FindSelectedPort(const FIntPoint& Seed) const
{
    const int32 Index = SelectedPortIndex(Seed);
    return Index != INDEX_NONE && Ports[Index].bReady ? &Ports[Index].Port : nullptr;
}

EVoxelHydrologySharedSearchStep FVoxelHydrologyWidthRoutingQuery::AdvanceRestoreSelectedPort(
    const FIntPoint& Seed, int64 UpstreamW, int64 DownstreamW, int32 WorkBudget, FString& OutError, const TAtomic<bool>* Cancel)
{
    OutError.Reset();
    if (Cancel && Cancel->Load()) return EVoxelHydrologySharedSearchStep::Canceled;
    if (!bInitialized) { OutError = TEXT("Width routing query is not initialized"); return EVoxelHydrologySharedSearchStep::Failed; }
    if (bRestoreStarted && !bRestoreReady && (Seed != RestoreSeed || UpstreamW != RestoreUpstreamW || DownstreamW != RestoreDownstreamW))
    { OutError = TEXT("Finish the pending selected-port restoration before changing its inputs"); return EVoxelHydrologySharedSearchStep::Failed; }
    if (bRestoreReady && Seed == RestoreSeed && UpstreamW == RestoreUpstreamW && DownstreamW == RestoreDownstreamW) return EVoxelHydrologySharedSearchStep::Ready;
    const auto* Label = Find(Seed);
    if (!Label) { OutError = TEXT("Request the upstream routing seed before restoring its port"); return EVoxelHydrologySharedSearchStep::NeedInput; }
    const int32 PortIndex = SelectedPortIndex(Seed);
    if (PortIndex == INDEX_NONE) { OutError = TEXT("Routing seed has no selected port to restore"); return EVoxelHydrologySharedSearchStep::Failed; }
    const auto* Downstream = Find(Label->Parent);
    if (!Downstream) { OutError = TEXT("Request the downstream routing seed before restoring its port"); return EVoxelHydrologySharedSearchStep::NeedInput; }
    if (!Downstream->bEligible || Downstream->W > DownstreamW || UpstreamW < Label->W || UpstreamW < DownstreamW)
    { OutError = TEXT("Restoration height is not the certified downstream water level"); return EVoxelHydrologySharedSearchStep::Failed; }
    if (WorkBudget <= 0) return EVoxelHydrologySharedSearchStep::Yielded;
    if (!bRestoreStarted || bRestoreReady)
    {
        Restore.FreeBacking(); RestoredHeights.Reset();
        RestoreSeed = Seed; RestoreDownstreamW = DownstreamW; RestoreUpstreamW=RestoreCurrentW=UpstreamW; RestorePortIndex = PortIndex;
        RestoreInterval = RestoreRepeat = 0; bRestoreStarted = true; bRestoreLogicalReady = bRestoreReady = false;
    }
    const auto& Port = Ports[RestorePortIndex].Port;
    const auto& Intervals = Port.GetIntervals();
    const auto& Counts = Port.GetExpansionCounts();
    if (Intervals.Num() != Counts.Num() || Intervals.IsEmpty())
    { OutError = TEXT("Invalid selected-port equality expansion"); return EVoxelHydrologySharedSearchStep::Failed; }
    if (!bRestoreLogicalReady)
    {
        const auto Step = Restore.Advance(Intervals, DownstreamW, WorkBudget, true, Cancel && Cancel->Load());
        if (Step == EVoxelHydrologyWidthStep::Pending) return EVoxelHydrologySharedSearchStep::Yielded;
        if (Step == EVoxelHydrologyWidthStep::CapacityBlocked) return EVoxelHydrologySharedSearchStep::CapacityBlocked;
        if (Step == EVoxelHydrologyWidthStep::Canceled) return EVoxelHydrologySharedSearchStep::Canceled;
        if (Step != EVoxelHydrologyWidthStep::Ready)
        { OutError = TEXT("Certified selected port could not restore downstream water"); return EVoxelHydrologySharedSearchStep::Failed; }
        bRestoreLogicalReady = true;
        return EVoxelHydrologySharedSearchStep::Yielded;
    }
    const auto& Heights = Restore.GetHeights();
    while (RestoreInterval < Heights.Num() && WorkBudget > 0)
    {
        --WorkBudget;
        if (Cancel && Cancel->Load()) return EVoxelHydrologySharedSearchStep::Canceled;
        if (Counts[RestoreInterval] <= 0) { OutError = TEXT("Invalid selected-port repeat count"); return EVoxelHydrologySharedSearchStep::Failed; }
        if (RestoredHeights.Num() == MAX_int32) { OutError = TEXT("Oversized: selected-port unit heights"); return EVoxelHydrologySharedSearchStep::Failed; }
        if (RestoredHeights.Num() == RestoredHeights.Max() && !VoxelGenerationAllocation::RetainArray(
            RestoredHeights, WidthRoutingCapacity(RestoredHeights.Max()), OutError)) return EVoxelHydrologySharedSearchStep::CapacityBlocked;
        const int64 Height=RestoreInterval==Heights.Num()-1 ? DownstreamW : FMath::Min(RestoreCurrentW,Intervals[RestoreInterval].Upper);
        if (Height<Heights[RestoreInterval] || Height>Intervals[RestoreInterval].Upper || Height>RestoreCurrentW)
        { OutError=TEXT("Invalid highest-feasible selected-port profile"); return EVoxelHydrologySharedSearchStep::Failed; }
        RestoredHeights.Add(Height); RestoreCurrentW=Height;
        if (++RestoreRepeat == Counts[RestoreInterval]) { RestoreRepeat = 0; ++RestoreInterval; }
    }
    if (RestoreInterval < Heights.Num()) return EVoxelHydrologySharedSearchStep::Yielded;
    const auto& Edge = Ports[RestorePortIndex].Result;
    const int64 Length = FMath::Abs(int64(Edge.Downstream.X) - RestoreSeed.X) + FMath::Abs(int64(Edge.Downstream.Y) - RestoreSeed.Y);
    if (int64(RestoredHeights.Num()) != Length + 1 || RestoredHeights[0] != UpstreamW ||
        RestoredHeights[RestoredHeights.Num() - 1] != DownstreamW)
    { OutError = TEXT("Selected-port restored unit heights do not match its certified endpoints"); return EVoxelHydrologySharedSearchStep::Failed; }
    bRestoreReady = true; return EVoxelHydrologySharedSearchStep::Ready;
}

uint64 FVoxelHydrologyWidthRoutingQuery::GetAllocatedBytes() const
{
    uint64 Bytes = VoxelGenerationAllocation::AllocatedBytes(Nodes) + VoxelGenerationAllocation::AllocatedBytes(ByPosition) +
        VoxelGenerationAllocation::AllocatedBytes(Ports) + VoxelGenerationAllocation::AllocatedBytes(RestoredHeights) +
        Search.GetAllocatedBytes() + Restore.GetAllocatedBytes();
    for (const auto& Port : Ports) Bytes += Port.Port.GetAllocatedBytes();
    return Bytes;
}
void FVoxelHydrologyWidthRoutingQuery::FreeBacking()
{
    Search.FreeBacking(); Restore.FreeBacking();
    for (auto& Port : Ports) Port.Port.FreeBacking();
    VoxelGenerationAllocation::FreeRetainedArray(Ports);
    VoxelGenerationAllocation::FreeRetainedArray(Nodes);
    VoxelGenerationAllocation::FreeRetainedMap(ByPosition);
    VoxelGenerationAllocation::FreeRetainedArray(RestoredHeights);
    NodeMapCapacity = EqualityHalfLength = MinimumGround = 0;
    RestorePortIndex = INDEX_NONE; RestoreInterval = RestoreRepeat = 0; RecipeIdentity = 0;
    bInitialized = bSearchActive = bRestoreStarted = bRestoreLogicalReady = bRestoreReady = false;
}
