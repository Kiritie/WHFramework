#include "Voxel/Generation/Hydrology/VoxelHydrologyWidthTransfer.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

namespace
{
    int64 WidthMaxCapacity(int32 Num)
    { return int64(Num) * 2 + 16 > MAX_int32 ? MAX_int32 : int64(Num) * 2 + 16; }
    template<typename T> bool WidthRoom(TArray<T>& Array)
    {
        if (Array.Num() < Array.Max()) return true;
        FString Error;
        const int64 Capacity = WidthMaxCapacity(Array.Num());
        return VoxelGenerationAllocation::RetainArray(Array, int32(Capacity), Error);
    }
    int64 WidthMax(int64 A, int64 B) { return A > B ? A : B; }
    int64 WidthMin(int64 A, int64 B) { return A < B ? A : B; }
    bool WidthCellLess(const FIntPoint& A, const FIntPoint& B)
    { return A.X != B.X ? A.X < B.X : A.Y < B.Y; }
}
void FVoxelHydrologyWidthComposeState::Reset() { *this = FVoxelHydrologyWidthComposeState(); }
EVoxelHydrologyWidthStep FVoxelHydrologyWidthComposeState::Advance(
    const TArray<FVoxelHydrologyWidthNodeInterval>& Path, int32 WorkBudget,
    bool bInputComplete, bool bCancel, FVoxelHydrologyWidthEdgeTransfer& OutTransfer)
{
    if (bCancel) return EVoxelHydrologyWidthStep::Canceled;
    if (!bInputComplete) return EVoxelHydrologyWidthStep::NeedInput;
    if (Status == EVoxelHydrologyWidthStep::Ready) { OutTransfer = Candidate; return Status; }
    if (Status != EVoxelHydrologyWidthStep::Pending) return Status;
    if (!bStarted)
    {
        if (Path.Num() == 0) return EVoxelHydrologyWidthStep::NeedInput;
        if (WorkBudget <= 0) return Status;
        bStarted = true; Count = Path.Num(); Cursor = Count - 1;
        Candidate.DownLower = Path[Cursor].Lower;
        Candidate.B = Path[Cursor].Upper;
    }
    if (Path.Num() != Count) return EVoxelHydrologyWidthStep::InvalidInput;
    while (WorkBudget > 0 && Cursor >= 0)
    {
        --WorkBudget;
        const auto& Node = Path[Cursor];
        if (Node.Lower > Node.Upper)
            return Status = EVoxelHydrologyWidthStep::CertifiedInfeasible;
        if (Cursor != Count - 1)
        {
            Candidate.A = Candidate.bHasRaise ? WidthMax(Candidate.A, Node.Lower) : Node.Lower;
            Candidate.bHasRaise = true;
            if (Candidate.A > Node.Upper)
                return Status = EVoxelHydrologyWidthStep::CertifiedInfeasible;
        }
        Candidate.B = WidthMin(Candidate.B, Node.Upper);
        --Cursor;
    }
    if (Cursor < 0)
    {
        if (Candidate.DownLower > Candidate.B)
            return Status = EVoxelHydrologyWidthStep::CertifiedInfeasible;
        Candidate.bFeasible = true;
        OutTransfer = Candidate;
        return Status = EVoxelHydrologyWidthStep::Ready;
    }
    return Status;
}
void FVoxelHydrologyWidthRestoreState::Reset()
{ FreeBacking(); *this = FVoxelHydrologyWidthRestoreState(); }
uint64 FVoxelHydrologyWidthRestoreState::GetAllocatedBytes() const
{ return VoxelGenerationAllocation::AllocatedBytes(Heights); }
void FVoxelHydrologyWidthRestoreState::FreeBacking()
{
    VoxelGenerationAllocation::FreeRetainedArray(Heights);
    Cursor = INDEX_NONE; Count = 0; Water = InputWater = 0;
    bStarted = false; ReverseCursor = 0; Status = EVoxelHydrologyWidthStep::Pending;
}
EVoxelHydrologyWidthStep FVoxelHydrologyWidthRestoreState::Advance(
    const TArray<FVoxelHydrologyWidthNodeInterval>& Path, int64 DownWater,
    int32 WorkBudget, bool bInputComplete, bool bCancel)
{
    if (bCancel) return EVoxelHydrologyWidthStep::Canceled;
    if (!bInputComplete) return EVoxelHydrologyWidthStep::NeedInput;
    if (Status != EVoxelHydrologyWidthStep::Pending) return Status;
    if (!bStarted)
    {
        if (Path.Num() == 0) return EVoxelHydrologyWidthStep::NeedInput;
        if (WorkBudget <= 0) return Status;
        bStarted = true; Count = Path.Num(); Cursor = Count - 1;
        InputWater = Water = DownWater;
    }
    if (Path.Num() != Count || InputWater != DownWater) return EVoxelHydrologyWidthStep::InvalidInput;
    while (WorkBudget > 0 && Cursor >= 0)
    {
        --WorkBudget;
        const auto& Node = Path[Cursor];
        if (Node.Lower > Node.Upper || (Cursor == Count - 1 && Water < Node.Lower))
            return Status = EVoxelHydrologyWidthStep::CertifiedInfeasible;
        Water = WidthMax(Water, Node.Lower);
        if (Water > Node.Upper) return Status = EVoxelHydrologyWidthStep::CertifiedInfeasible;
        if (!WidthRoom(Heights)) return EVoxelHydrologyWidthStep::CapacityBlocked;
        Heights.Add(Water); --Cursor;
    }
    while (WorkBudget > 0 && Cursor < 0 && ReverseCursor < Count / 2)
    {
        --WorkBudget;
        const int64 Temp = Heights[ReverseCursor];
        Heights[ReverseCursor] = Heights[Count - 1 - ReverseCursor];
        Heights[Count - 1 - ReverseCursor] = Temp;
        ++ReverseCursor;
    }
    if (Cursor < 0 && ReverseCursor >= Count / 2) Status = EVoxelHydrologyWidthStep::Ready;
    return Status;
}
void FVoxelHydrologyWidthGraphSolver::Reset()
{ FreeBacking(); *this = FVoxelHydrologyWidthGraphSolver(); }
uint64 FVoxelHydrologyWidthGraphSolver::GetAllocatedBytes() const
{ return VoxelGenerationAllocation::AllocatedBytes(Labels) +
    VoxelGenerationAllocation::AllocatedBytes(Heap) + VoxelGenerationAllocation::AllocatedBytes(Positions) +
    VoxelGenerationAllocation::AllocatedBytes(IncomingHead) + VoxelGenerationAllocation::AllocatedBytes(IncomingNext); }
void FVoxelHydrologyWidthGraphSolver::FreeBacking()
{
    VoxelGenerationAllocation::FreeRetainedArray(Labels);
    VoxelGenerationAllocation::FreeRetainedArray(Heap);
    VoxelGenerationAllocation::FreeRetainedArray(Positions);
    VoxelGenerationAllocation::FreeRetainedArray(IncomingHead);
    VoxelGenerationAllocation::FreeRetainedArray(IncomingNext);
    NodeCount = EdgeCount = Cursor = 0; Stage = HeapMode = 0;
    Current = CurrentEdge = PreviousCoordinate = INDEX_NONE;
    Status = EVoxelHydrologyWidthStep::Pending;
}
bool FVoxelHydrologyWidthGraphSolver::Tight(const FVoxelHydrologyWidthEdgeTransfer& E) const
{
    if (!E.bFeasible) return false;
    const auto& Up = Labels[E.Upstream]; const auto& Down = Labels[E.Downstream];
    return Up.bEligible && Down.bEligible && Down.W >= E.DownLower && Down.W <= E.B &&
        Up.W == (E.bHasRaise ? WidthMax(Down.W, E.A) : Down.W);
}
bool FVoxelHydrologyWidthGraphSolver::BetterParent(
    const TArray<FVoxelHydrologyWidthGraphNode>& Nodes,
    const TArray<FVoxelHydrologyWidthEdgeTransfer>& Edges, int32 NewIndex, int32 OldIndex) const
{
    if (OldIndex == INDEX_NONE) return true;
    const auto& New = Edges[NewIndex]; const auto& Old = Edges[OldIndex];
    const FIntPoint& A = Nodes[New.Downstream].Cell; const FIntPoint& B = Nodes[Old.Downstream].Cell;
    if (A.X != B.X || A.Y != B.Y) return WidthCellLess(A, B);
    if (New.PathId != Old.PathId) return New.PathId < Old.PathId;
    return NewIndex < OldIndex;
}
bool FVoxelHydrologyWidthGraphSolver::HeapLess(int32 A, int32 B,
    const TArray<FVoxelHydrologyWidthGraphNode>& Nodes) const
{
    if (HeapMode == 1 && Labels[A].W != Labels[B].W) return Labels[A].W < Labels[B].W;
    if (HeapMode == 2 && Labels[A].D != Labels[B].D) return Labels[A].D < Labels[B].D;
    if (Nodes[A].Cell.X != Nodes[B].Cell.X || Nodes[A].Cell.Y != Nodes[B].Cell.Y)
        return WidthCellLess(Nodes[A].Cell, Nodes[B].Cell);
    return A < B;
}
void FVoxelHydrologyWidthGraphSolver::HeapUpdate(int32 Node,
    const TArray<FVoxelHydrologyWidthGraphNode>& Nodes)
{
    int32 Position = Positions[Node];
    if (Position == INDEX_NONE) { Position = Heap.Num(); Heap.Add(Node); Positions[Node] = Position; }
    while (Position > 0)
    {
        const int32 Parent = (Position - 1) / 2;
        if (!HeapLess(Heap[Position], Heap[Parent], Nodes)) break;
        const int32 Other = Heap[Parent]; Heap[Parent] = Node; Heap[Position] = Other;
        Positions[Node] = Parent; Positions[Other] = Position; Position = Parent;
    }
}
int32 FVoxelHydrologyWidthGraphSolver::HeapPop(const TArray<FVoxelHydrologyWidthGraphNode>& Nodes)
{
    const int32 Result = Heap[0]; const int32 Last = Heap[Heap.Num() - 1];
    Heap.RemoveAt(Heap.Num() - 1, 1, EAllowShrinking::No); Positions[Result] = INDEX_NONE;
    if (Heap.Num() == 0) return Result;
    Heap[0] = Last; Positions[Last] = 0; int32 Position = 0;
    while (int64(Position) * 2 + 1 < Heap.Num())
    {
        int32 Child = Position * 2 + 1;
        if (Child + 1 < Heap.Num() && HeapLess(Heap[Child + 1], Heap[Child], Nodes)) ++Child;
        if (!HeapLess(Heap[Child], Last, Nodes)) break;
        Heap[Position] = Heap[Child]; Positions[Heap[Position]] = Position;
        Heap[Child] = Last; Positions[Last] = Child; Position = Child;
    }
    return Result;
}
EVoxelHydrologyWidthStep FVoxelHydrologyWidthGraphSolver::Advance(
    const TArray<FVoxelHydrologyWidthGraphNode>& Nodes,
    const TArray<FVoxelHydrologyWidthEdgeTransfer>& Edges,
    bool bDomainClosed, int32 WorkBudget, bool bCancel)
{
    if (bCancel) return EVoxelHydrologyWidthStep::Canceled;
    if (!bDomainClosed) return EVoxelHydrologyWidthStep::NeedInput;
    if (Status != EVoxelHydrologyWidthStep::Pending) return Status;
    if (Stage == 0 && Cursor == 0 && Labels.Num() == 0)
    { NodeCount = Nodes.Num(); EdgeCount = Edges.Num(); }
    if (Nodes.Num() != NodeCount || Edges.Num() != EdgeCount)
        return Status = EVoxelHydrologyWidthStep::InvalidInput;
    while (WorkBudget > 0)
    {

        if (Stage == 0)
        {
            if (Cursor == NodeCount) { Stage = 1; Cursor = 0; continue; }
            --WorkBudget; const auto& N = Nodes[Cursor];
            if (N.Interval.Lower > N.Interval.Upper ||
                (N.bFixed && (N.FixedH < N.Interval.Lower || N.FixedH > N.Interval.Upper)))
                return Status = EVoxelHydrologyWidthStep::InvalidInput;
            if (!WidthRoom(Labels) || !WidthRoom(Heap) || !WidthRoom(Positions) || !WidthRoom(IncomingHead))
                return EVoxelHydrologyWidthStep::CapacityBlocked;
            FVoxelHydrologyWidthGraphLabel L;
            if (N.bFixed) { L.W = N.FixedH; L.bEligible = true; L.bHasDistance = true; }
            Labels.Add(L); Positions.Add(INDEX_NONE); IncomingHead.Add(INDEX_NONE);
            HeapUpdate(Cursor, Nodes); ++Cursor;
        }
        else if (Stage == 1)
        {
            if (Cursor == EdgeCount) { Stage = 2; Cursor = 0; continue; }
            --WorkBudget; const auto& E = Edges[Cursor];
            if (E.Upstream < 0 || E.Downstream < 0 || E.Upstream >= NodeCount || E.Downstream >= NodeCount)
                return Status = EVoxelHydrologyWidthStep::InvalidInput;
            if (E.bFeasible)
            {
                const auto& U = Nodes[E.Upstream].Interval; const auto& D = Nodes[E.Downstream].Interval;
                if (E.DownLower != D.Lower || E.DownLower > E.B || E.B > D.Upper || E.B > U.Upper ||
                    (E.bHasRaise && (E.A < U.Lower || E.A > U.Upper)) ||
                    (!E.bHasRaise && E.DownLower < U.Lower))
                    return Status = EVoxelHydrologyWidthStep::InvalidInput;
            }
            if (!WidthRoom(IncomingNext)) return EVoxelHydrologyWidthStep::CapacityBlocked;
            IncomingNext.Add(E.bFeasible ? IncomingHead[E.Downstream] : INDEX_NONE);
            if (E.bFeasible) IncomingHead[E.Downstream] = Cursor;
            ++Cursor;
        }
        else if (Stage == 2)
        {
            if (Heap.Num() == 0) { Stage = 3; Cursor = 0; HeapMode = 1; continue; }
            --WorkBudget; const int32 Next = HeapPop(Nodes);
            if (PreviousCoordinate != INDEX_NONE)
            {
                const auto& A = Nodes[PreviousCoordinate].Cell; const auto& B = Nodes[Next].Cell;
                if (A.X == B.X && A.Y == B.Y) return Status = EVoxelHydrologyWidthStep::InvalidInput;
            }
            PreviousCoordinate = Next;
        }
        else if (Stage == 3 || Stage == 6)
        {
            if (Cursor == NodeCount) { ++Stage; Cursor = 0; continue; }
            --WorkBudget;
            if ((Stage == 3 && Nodes[Cursor].bFixed) || (Stage == 6 && Labels[Cursor].bHasDistance))
                HeapUpdate(Cursor, Nodes);
            ++Cursor;
        }
        else if (Stage == 4 || Stage == 7)
        {
            if (Current == INDEX_NONE)
            {
                if (Heap.Num() == 0)
                { ++Stage; Cursor = 0; if (Stage == 5) HeapMode = 2; continue; }
                --WorkBudget; Current = HeapPop(Nodes); CurrentEdge = IncomingHead[Current];
                continue;
            }
            if (CurrentEdge == INDEX_NONE) { Current = INDEX_NONE; continue; }
            --WorkBudget; const int32 EdgeIndex = CurrentEdge;
            const auto& E = Edges[EdgeIndex]; CurrentEdge = IncomingNext[EdgeIndex];
            if (Nodes[E.Upstream].bFixed) continue;
            auto& U = Labels[E.Upstream]; const auto& D = Labels[E.Downstream];
            if (Stage == 4)
            {
                if (!D.bEligible || D.W < E.DownLower || D.W > E.B) continue;
                const int64 Candidate = E.bHasRaise ? WidthMax(D.W, E.A) : D.W;
                if (Candidate < Nodes[E.Upstream].Interval.Lower || Candidate > Nodes[E.Upstream].Interval.Upper) continue;
                if (!U.bEligible || Candidate < U.W)
                { U.W = Candidate; U.bEligible = true; HeapUpdate(E.Upstream, Nodes); }
            }
            else if (Tight(E) && U.W == D.W && D.bHasDistance && D.D < NodeCount &&
                (!U.bHasDistance || D.D + 1 < U.D))
            { U.D = D.D + 1; U.bHasDistance = true; HeapUpdate(E.Upstream, Nodes); }
        }
        else if (Stage == 5)
        {
            if (Cursor == EdgeCount) { Stage = 6; Cursor = 0; continue; }
            --WorkBudget; const auto& E = Edges[Cursor++];
            if (Tight(E) && Labels[E.Downstream].W < Labels[E.Upstream].W)
            { Labels[E.Upstream].D = 0; Labels[E.Upstream].bHasDistance = true; }
        }
        else if (Stage == 8)
        {
            if (Cursor == EdgeCount) { Stage = 9; Cursor = 0; continue; }
            const int32 Index = Cursor++; --WorkBudget; const auto& E = Edges[Index];
            if (!Tight(E) || Nodes[E.Upstream].bFixed) continue;
            auto& U = Labels[E.Upstream]; const auto& D = Labels[E.Downstream];
            if (!U.bHasDistance || !D.bHasDistance) continue;
            const bool bExit = D.W < U.W && U.D == 0;
            const bool bPlateau = D.W == U.W && D.D < NodeCount && U.D == D.D + 1;
            if ((bExit || bPlateau) && BetterParent(Nodes, Edges, Index, U.ParentEdge))
            { U.Parent = E.Downstream; U.ParentEdge = Index; }
        }
        else
        {
            if (Cursor == NodeCount) return Status = EVoxelHydrologyWidthStep::Ready;
            --WorkBudget; const int32 Index = Cursor++; const auto& L = Labels[Index];
            if (L.bEligible && (!L.bHasDistance || (!Nodes[Index].bFixed && L.Parent == INDEX_NONE)))
                return Status = EVoxelHydrologyWidthStep::InvalidInput;
        }
    }
    return Status;
}
