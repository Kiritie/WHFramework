#include "Voxel/Generation/Hydrology/VoxelHydrologyBucketOutlet.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

namespace
{
    EVoxelHydrologySharedSearchStep BucketFailure(const FString& Error)
    {
        if (Error == TEXT("Canceled")) return EVoxelHydrologySharedSearchStep::Canceled;
        if (Error.StartsWith(TEXT("CapacityBlocked"))) return EVoxelHydrologySharedSearchStep::CapacityBlocked;
        if (Error.StartsWith(TEXT("Yielded"))) return EVoxelHydrologySharedSearchStep::Yielded;
        return EVoxelHydrologySharedSearchStep::Failed;
    }
    bool BucketPositionLess(const FIntPoint& A, const FIntPoint& B)
    { return A.X != B.X ? A.X < B.X : A.Y < B.Y; }
}
bool FVoxelHydrologyBucketOutlet::Initialize(const FIntPoint& InAnchor,
    const FIntPoint& InMinimum, const FIntPoint& InMaximum, FString& Error)
{
    const int64 W = int64(InMaximum.X) - InMinimum.X + 1;
    const int64 H = int64(InMaximum.Y) - InMinimum.Y + 1;
    if (bInitialized || W <= 0 || H <= 0 || InAnchor.X < InMinimum.X || InAnchor.X > InMaximum.X ||
        InAnchor.Y < InMinimum.Y || InAnchor.Y > InMaximum.Y)
    { Error = TEXT("Invalid bucket outlet domain"); return false; }
    if (W > MAX_int32 || H > MAX_int32 || W * H > MAX_int32)
    { Error = TEXT("Oversized: bucket outlet domain"); return false; }
    Anchor = InAnchor; Minimum = InMinimum; Maximum = InMaximum;
    Width = int32(W); Count = int32(W * H);
    AnchorIndex = int32((int64(Anchor.Y) - Minimum.Y) * Width + int64(Anchor.X) - Minimum.X);
    bInitialized = true; Error.Reset(); return true;
}
FIntPoint FVoxelHydrologyBucketOutlet::Position(int32 Index) const
{ return FIntPoint(int32(int64(Minimum.X) + Index % Width), int32(int64(Minimum.Y) + Index / Width)); }
int32 FVoxelHydrologyBucketOutlet::Neighbor(int32 Index, int32 Face) const
{

    if (Face == 0) return Index % Width ? Index - 1 : INDEX_NONE;
    if (Face == 1) return Index >= Width ? Index - Width : INDEX_NONE;
    if (Face == 2) return int64(Index) + Width < Count ? Index + Width : INDEX_NONE;
    return Index % Width + 1 < Width ? Index + 1 : INDEX_NONE;
}
bool FVoxelHydrologyBucketOutlet::HeapLess(int32 A, int32 B) const
{
    return Cells[A].Barrier != Cells[B].Barrier ? Cells[A].Barrier < Cells[B].Barrier :
        BucketPositionLess(Position(A), Position(B));
}
void FVoxelHydrologyBucketOutlet::HeapRaise(int32 Index)
{
    int32 At = Cells[Index].HeapIndex;
    if (At == INDEX_NONE) { At = Heap.Add(Index); Cells[Index].HeapIndex = At; }
    while (At > 0)
    {
        const int32 Parent = (At - 1) / 2;
        if (!HeapLess(Heap[At], Heap[Parent])) break;
        Swap(Heap[At], Heap[Parent]);
        Cells[Heap[At]].HeapIndex = At; Cells[Heap[Parent]].HeapIndex = Parent; At = Parent;
    }
}
int32 FVoxelHydrologyBucketOutlet::HeapPop()
{
    const int32 ResultIndex = Heap[0];
    Heap[0] = Heap.Last(); Heap.RemoveAt(Heap.Num() - 1, 1, EAllowShrinking::No);
    Cells[ResultIndex].HeapIndex = INDEX_NONE;
    if (Heap.IsEmpty()) return ResultIndex;
    Cells[Heap[0]].HeapIndex = 0;
    int32 At = 0;
    while (int64(At) * 2 + 1 < Heap.Num())
    {
        int32 Child = At * 2 + 1;
        if (Child + 1 < Heap.Num() && HeapLess(Heap[Child + 1], Heap[Child])) ++Child;
        if (!HeapLess(Heap[Child], Heap[At])) break;
        Swap(Heap[At], Heap[Child]);
        Cells[Heap[At]].HeapIndex = At; Cells[Heap[Child]].HeapIndex = Child; At = Child;
    }
    return ResultIndex;
}
void FVoxelHydrologyBucketOutlet::Hash(uint64 Value)
{

    for (int32 Byte = 0; Byte < 8; ++Byte)
    { PathHash ^= (Value >> (Byte * 8)) & 255; PathHash *= 1099511628211ull; }
}
EVoxelHydrologySharedSearchStep FVoxelHydrologyBucketOutlet::Advance(FReadSample Read,
    int32 WorkBudget, FString& Error, const TAtomic<bool>* Cancel)
{
    Error.Reset();
    if (!bInitialized) { Error = TEXT("Bucket outlet not initialized"); return EVoxelHydrologySharedSearchStep::Failed; }
    if (Cancel && Cancel->Load()) { Error = TEXT("Canceled"); return EVoxelHydrologySharedSearchStep::Canceled; }
    if (bReady) return EVoxelHydrologySharedSearchStep::Ready;
    while (WorkBudget > 0)
    {
        --WorkBudget;
        if (Cancel && Cancel->Load()) { Error = TEXT("Canceled"); return EVoxelHydrologySharedSearchStep::Canceled; }
        if (Stage == 0)
        {


            if (Cells.Max() < Count && !VoxelGenerationAllocation::RetainArray(Cells, Count, Error)) return BucketFailure(Error);
            if (Heap.Max() < Count && !VoxelGenerationAllocation::RetainArray(Heap, Count, Error)) return BucketFailure(Error);
            if (Queue.Max() < Count && !VoxelGenerationAllocation::RetainArray(Queue, Count, Error)) return BucketFailure(Error);
            if (Path.Max() < Count && !VoxelGenerationAllocation::RetainArray(Path, Count, Error)) return BucketFailure(Error);
            Stage = 1;
        }
        else if (Stage == 1)
        {
            if (Cursor == Count)
            {
                if (!WetCount) { bReady = true; return EVoxelHydrologySharedSearchStep::Ready; }
                Cells[AnchorIndex].Barrier = Cells[AnchorIndex].Sample.ExistingWaterZ != MIN_int32 ?
                    Cells[AnchorIndex].Sample.ExistingWaterZ : Cells[AnchorIndex].Sample.OriginalGround;
                HeapRaise(AnchorIndex); Stage = 2; continue;
            }
            FVoxelHydrologySectionSample Sample;
            const auto Status = Read(Position(Cursor), Sample, Error);
            if (Status == EVoxelHydrologyClosureRead::Pending)
            {
                if (Error.StartsWith(TEXT("Yielded")) || Error.StartsWith(TEXT("CapacityBlocked")) || Error == TEXT("Canceled"))
                    return BucketFailure(Error);
                return EVoxelHydrologySharedSearchStep::NeedInput;
            }
            if (Status != EVoxelHydrologyClosureRead::Ready) return BucketFailure(Error);
            if (Sample.OriginalGround < -32768 || Sample.OriginalGround > 32768 ||
                (Sample.ExistingWaterZ != MIN_int32 && (Sample.ExistingWaterZ < -32768 || Sample.ExistingWaterZ > 32768 ||
                 Sample.OriginalGround >= Sample.ExistingWaterZ || (Sample.ExistingBodyId.High == 0 && Sample.ExistingBodyId.Low == 0))))
            { Error = TEXT("Invalid bucket original terrain or certified water"); return EVoxelHydrologySharedSearchStep::Failed; }
            FCell Cell; Cell.Sample = Sample; Cells.Add(Cell);
            if (Sample.ExistingWaterZ != MIN_int32) ++WetCount;
            ++Cursor;
        }
        else if (Stage == 2)
        {
            if (Heap.IsEmpty()) { bReady = true; return EVoxelHydrologySharedSearchStep::Ready; }
            const int32 Current = HeapPop(); const auto& Cell = Cells[Current];
            if (Cell.Sample.ExistingWaterZ != MIN_int32)
            {
                Threshold = Cell.Barrier;
                Cells[AnchorIndex].Distance = 0; Queue.Add(AnchorIndex); Cursor = 0; Stage = 3; continue;
            }
            for (int32 Face = 0; Face < 4; ++Face)
            {
                const int32 Next = Neighbor(Current, Face); if (Next == INDEX_NONE) continue;
                auto& Other = Cells[Next];
                const int32 Height = Other.Sample.ExistingWaterZ != MIN_int32 ? Other.Sample.ExistingWaterZ : Other.Sample.OriginalGround;
                const int32 Candidate = FMath::Max(Cell.Barrier, Height);
                if (Candidate < Other.Barrier) { Other.Barrier = Candidate; HeapRaise(Next); }
            }
        }
        else if (Stage == 3)
        {
            if (Cursor == Queue.Num() || (Chosen != INDEX_NONE && Cells[Queue[Cursor]].Distance > Cells[Chosen].Distance))
            {
                if (Chosen == INDEX_NONE) { Error = TEXT("Invalid bucket threshold path"); return EVoxelHydrologySharedSearchStep::Failed; }
                Cursor = Chosen; Stage = 4; continue;
            }
            const int32 Current = Queue[Cursor++]; const auto& Cell = Cells[Current];
            if (Cell.Sample.ExistingWaterZ != MIN_int32)
            {
                if (Chosen == INDEX_NONE || BucketPositionLess(Position(Current), Position(Chosen))) Chosen = Current;
                continue;
            }
            if (Chosen != INDEX_NONE) continue;
            for (int32 Face = 0; Face < 4; ++Face)
            {
                const int32 Next = Neighbor(Current, Face); if (Next == INDEX_NONE) continue;
                auto& Other = Cells[Next];
                const int32 Height = Other.Sample.ExistingWaterZ != MIN_int32 ? Other.Sample.ExistingWaterZ : Other.Sample.OriginalGround;
                if (Other.Distance == INDEX_NONE && Height <= Threshold)
                { Other.Distance = Cell.Distance + 1; Other.Parent = Current; Queue.Add(Next); }
            }
        }
        else if (Stage == 4)
        {
            if (Cursor == INDEX_NONE)
            {
                if (Path.IsEmpty() || Path.Last() != Anchor)
                { Error = TEXT("Invalid bucket witness ancestry"); return EVoxelHydrologySharedSearchStep::Failed; }
                Cursor = 0; Stage = 5; continue;
            }
            if (Path.Num() >= Count) { Error = TEXT("Invalid bucket witness cycle"); return EVoxelHydrologySharedSearchStep::Failed; }
            Path.Add(Position(Cursor)); Cursor = Cells[Cursor].Parent;
        }
        else if (Stage == 5)
        {
            if (Cursor < Path.Num() / 2) { Swap(Path[Cursor], Path[Path.Num() - 1 - Cursor]); ++Cursor; }
            else
            {
                Cursor = 0; Stage = 6;
                Hash(uint32(Threshold)); Hash(uint32(Cells[Chosen].Sample.ExistingWaterZ));
                Hash(Cells[Chosen].Sample.ExistingBodyId.High); Hash(Cells[Chosen].Sample.ExistingBodyId.Low);
            }
        }
        else
        {
            if (Cursor < Path.Num())
            {
                const auto& P = Path[Cursor++]; Hash(uint32(P.X)); Hash(uint32(P.Y));
                const int32 I = int32((int64(P.Y) - Minimum.Y) * Width + int64(P.X) - Minimum.X);
                Hash(uint32(Cells[I].Sample.OriginalGround));
            }
            else
            {
                Result.Terminal = Position(Chosen); Result.WaterZ = Cells[Chosen].Sample.ExistingWaterZ;
                Result.BodyId = Cells[Chosen].Sample.ExistingBodyId; Result.Barrier = Threshold;
                Result.PathLength = Path.Num() - 1; Result.Witness = PathHash ? PathHash : 1;
                Result.bHasOutlet = true; bReady = true; return EVoxelHydrologySharedSearchStep::Ready;
            }
        }
    }
    return EVoxelHydrologySharedSearchStep::Yielded;
}
uint64 FVoxelHydrologyBucketOutlet::GetAllocatedBytes() const
{
    return VoxelGenerationAllocation::AllocatedBytes(Cells) + VoxelGenerationAllocation::AllocatedBytes(Heap) +
        VoxelGenerationAllocation::AllocatedBytes(Queue) + VoxelGenerationAllocation::AllocatedBytes(Path);
}
void FVoxelHydrologyBucketOutlet::FreeBacking()
{
    VoxelGenerationAllocation::FreeRetainedArray(Cells); VoxelGenerationAllocation::FreeRetainedArray(Heap);
    VoxelGenerationAllocation::FreeRetainedArray(Queue); VoxelGenerationAllocation::FreeRetainedArray(Path);
    Width = Count = AnchorIndex = Stage = Cursor = WetCount = 0;
    Chosen = INDEX_NONE; Threshold = MAX_int32; PathHash = 1469598103934665603ull;
    Result = FVoxelHydrologyBucketOutletResult(); bInitialized = bReady = false;
}
