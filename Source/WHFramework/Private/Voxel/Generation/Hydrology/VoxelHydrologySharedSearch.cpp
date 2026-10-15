#include "Voxel/Generation/Hydrology/VoxelHydrologySharedSearch.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

namespace
{
    int32 SharedSearchCapacity(int32 Required, int32 Previous)
    {
        return int32(FMath::Min<int64>(MAX_int32,
            FMath::Max<int64>(Required, FMath::Max<int64>(16, int64(Previous) * 2))));
    }
    bool SharedSearchCellLess(const FIntPoint& A, const FIntPoint& B)
    {
        return A.X != B.X ? A.X < B.X : A.Y < B.Y;
    }
}

bool FVoxelHydrologySharedSearch::Initialize(const FIntPoint& InSeed,
    const FIntPoint& InMinimum, const FIntPoint& InMaximumInclusive, FString& OutError,
    EVoxelHydrologySharedSearchPurpose InPurpose)
{
    if (bInitialized || InMinimum.X > InMaximumInclusive.X || InMinimum.Y > InMaximumInclusive.Y ||
        InSeed.X < InMinimum.X || InSeed.Y < InMinimum.Y ||
        InSeed.X > InMaximumInclusive.X || InSeed.Y > InMaximumInclusive.Y)
    {
        OutError = TEXT("Invalid shared drainage search initialization");
        return false;
    }
    Seed = InSeed;
    Minimum = InMinimum;
    Maximum = InMaximumInclusive;
    bFineWaterOnly = InPurpose == EVoxelHydrologySharedSearchPurpose::FineWaterBody;
    bPreferDescendingWitness = bFineWaterOnly;
    bInitialized = true;
    OutError.Reset();
    return true;
}

bool FVoxelHydrologySharedSearch::Neighbor(const FIntPoint& Cell, int32 InFace, FIntPoint& OutCell) const
{
    static const FIntPoint Offsets[] = {FIntPoint(-1, 0), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(0, 1)};
    const int64 X = int64(Cell.X) + Offsets[InFace].X;
    const int64 Y = int64(Cell.Y) + Offsets[InFace].Y;
    if (X < Minimum.X || Y < Minimum.Y || X > Maximum.X || Y > Maximum.Y) return false;
    OutCell = FIntPoint(int32(X), int32(Y));
    return true;
}

bool FVoxelHydrologySharedSearch::EventLess(const FEvent& A, const FEvent& B) const
{
    if (A.Plane != B.Plane)
    {
        return A.Plane < B.Plane;
    }
    if (bPreferDescendingWitness && Cells[A.Index].GroundZ != Cells[B.Index].GroundZ)
    {
        return Cells[A.Index].GroundZ < Cells[B.Index].GroundZ;
    }
    return SharedSearchCellLess(Cells[A.Index].Cell, Cells[B.Index].Cell);
}

bool FVoxelHydrologySharedSearch::TryGetSeedDryCertificate(FVoxelHydrologySharedCell& OutCell) const
{
    if (Cells.IsEmpty() || Cells[0].IsReceiver() || Threshold > Cells[0].GroundZ)
    {
        return false;
    }
    // 已有通向真实湿接收体的路径，最高障碍不超过种子地面；只证明这一列为干地。
    OutCell = ReadCell(0);
    OutCell.SpillZ = OutCell.GroundZ;
    OutCell.bIncluded = true;
    OutCell.Parent = INDEX_NONE;
    OutCell.bParentIsLocalOutlet = false;
    return true;
}

int32 FVoxelHydrologySharedSearch::FindCellIndex(const FIntPoint& InCell) const
{
    check(bReady);
    const int32* Index = ByCell.Find(InCell);
    return Index ? *Index : INDEX_NONE;
}

int32 FVoxelHydrologySharedSearch::FCellView::Num() const
{
    return Search->Cells.Num();
}

FVoxelHydrologySharedCell FVoxelHydrologySharedSearch::FCellView::operator[](int32 InIndex) const
{
    return Search->ReadCell(InIndex);
}

const FVoxelHydrologySharedCell& FVoxelHydrologySharedSearch::GetDetails(const FSearchCell& InCell) const
{
    check(InCell.DetailIndex != INDEX_NONE);
    return Details[InCell.DetailIndex];
}

FVoxelHydrologySharedCell FVoxelHydrologySharedSearch::ReadCell(int32 InIndex) const
{
    const FSearchCell& Cell = Cells[InIndex];
    FVoxelHydrologySharedCell Result = Cell.DetailIndex != INDEX_NONE
        ? GetDetails(Cell) : FVoxelHydrologySharedCell();
    Result.Cell = Cell.Cell;
    Result.GroundZ = Cell.GroundZ;
    Result.RainWeight = Cell.RainWeight;
    Result.SpillZ = Cell.SpillZ;
    Result.PlateauDistance = Cell.PlateauDistance;
    Result.Parent = Cell.Parent;
    Result.bIncluded = Cell.bIncluded;
    Result.bHasLocalOutlet = Cell.bHasLocalOutlet;
    Result.bParentIsLocalOutlet = Cell.bParentIsLocalOutlet;
    Result.bHasCertifiedDryOutlet = Cell.bHasCertifiedDryOutlet;
    if (bFineWaterOnly && bReady && Cell.bIncluded)
    {
        // 首湿根确定种子最小溢流水位。仅种子、已到达干列与真实湿根带水位证明；
        // 其它低列必须由 FineWaterQuery 的严格低水位连通闭合确认，不能冒充整图证书。
        if (Cell.IsReceiver()) Result.SpillZ = Result.ReceiverWaterZ;
        else if (InIndex == 0 || Cell.GroundZ >= Threshold) Result.SpillZ = FMath::Max(Cell.GroundZ, Threshold);
    }
    return Result;
}

bool FVoxelHydrologySharedSearch::Push(const FEvent& Event, FString& OutError)
{
    if (Frontier.Num() == MAX_int32)
    { OutError = TEXT("Oversized: shared drainage frontier"); return false; }
    if (Frontier.Num() == Frontier.Max() && !VoxelGenerationAllocation::RetainArray(Frontier,
        SharedSearchCapacity(Frontier.Num() + 1, Frontier.Max()), OutError)) return false;
    int32 Cursor = Frontier.Add(Event);
    while (Cursor > 0)
    {
        const int32 Parent = (Cursor - 1) / 2;
        if (!EventLess(Frontier[Cursor], Frontier[Parent])) break;
        Swap(Frontier[Cursor], Frontier[Parent]);
        Cursor = Parent;
    }
    return true;
}

FVoxelHydrologySharedSearch::FEvent FVoxelHydrologySharedSearch::Pop()
{
    check(!Frontier.IsEmpty());
    const FEvent Result = Frontier[0];
    Frontier[0] = Frontier.Last();
    Frontier.RemoveAt(Frontier.Num() - 1, 1, EAllowShrinking::No);
    int32 Cursor = 0;
    while (int64(Cursor) * 2 + 1 < Frontier.Num())
    {
        int32 Child = Cursor * 2 + 1;
        if (Child + 1 < Frontier.Num() && EventLess(Frontier[Child + 1], Frontier[Child])) ++Child;
        if (!EventLess(Frontier[Child], Frontier[Cursor])) break;
        Swap(Frontier[Child], Frontier[Cursor]);
        Cursor = Child;
    }
    return Result;
}

bool FVoxelHydrologySharedSearch::Append(const FVoxelHydrologySharedCell& InCell,
    int32 InEntryPlane, int32& OutIndex, FString& OutError)
{
    if (Cells.Num() == MAX_int32)
    { OutError = TEXT("Oversized: shared drainage certificate"); return false; }
    if (!Cells.ReserveOne(OutError)) return false;
    const bool bNeedsDetails = InCell.IsReceiver() || InCell.bHasLocalOutlet;
    if (bNeedsDetails && !Details.ReserveOne(OutError)) return false;
    if (ByCell.Num() == MapCapacity)
    {
        const int32 Capacity = SharedSearchCapacity(MapCapacity + 1, MapCapacity);
        if (!VoxelGenerationAllocation::RetainMap(ByCell, Capacity, OutError)) return false;
        MapCapacity = Capacity;
    }


    if (Frontier.Num() == Frontier.Max() && !VoxelGenerationAllocation::RetainArray(Frontier,
        SharedSearchCapacity(Frontier.Num() + 1, Frontier.Max()), OutError)) return false;
    FSearchCell Cell;
    Cell.Cell = InCell.Cell;
    Cell.GroundZ = InCell.GroundZ;
    Cell.RainWeight = InCell.RainWeight;
    Cell.SpillZ = InCell.SpillZ;
    Cell.PlateauDistance = InCell.PlateauDistance;
    Cell.Parent = InCell.Parent;
    Cell.bIncluded = InCell.bIncluded;
    Cell.bHasLocalOutlet = InCell.bHasLocalOutlet;
    Cell.bParentIsLocalOutlet = InCell.bParentIsLocalOutlet;
    Cell.bReceiver = InCell.IsReceiver();
    Cell.bHasCertifiedDryOutlet = InCell.bHasCertifiedDryOutlet;
    if (bNeedsDetails) Cell.DetailIndex = Details.Add(InCell);
    OutIndex = Cells.Add(Cell);
    ByCell.Add(InCell.Cell, OutIndex);
    return Push(FEvent{OutIndex, InEntryPlane}, OutError);
}

EVoxelHydrologySharedSearchStep FVoxelHydrologySharedSearch::Advance(FReadCell InReadCell,
    int32 InFaceBudget, FString& OutError, const TAtomic<bool>* InCancel)
{
    auto Failure = [&]()
    {
        if (OutError == TEXT("Canceled")) return EVoxelHydrologySharedSearchStep::Canceled;
        if (OutError.StartsWith(TEXT("CapacityBlocked"))) return EVoxelHydrologySharedSearchStep::CapacityBlocked;
        return EVoxelHydrologySharedSearchStep::Failed;
    };
    OutError.Reset();
    if (!bInitialized) { OutError = TEXT("Shared drainage search was not initialized"); return Failure(); }
    if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return Failure(); }
    if (bReady) return EVoxelHydrologySharedSearchStep::Ready;
    if (InFaceBudget <= 0) return EVoxelHydrologySharedSearchStep::Yielded;
    auto ValidateInput = [&](FVoxelHydrologySharedCell& Cell)
    {
        if (Cell.bHasCertifiedDryOutlet && (!bFineWaterOnly || Cell.IsReceiver() || Cell.bHasLocalOutlet))
        {
            OutError = TEXT("Certified fine dry outlet cannot replace a wet receiver or runoff parent certificate");
            return false;
        }
        if (bFineWaterOnly && Cell.bHasLocalOutlet)
        {
            OutError = TEXT("Fine water requires physical wet receivers, not local runoff outlets");
            return false;
        }
        if (Cell.GroundZ < -32768 || Cell.GroundZ > 32768 || Cell.RainWeight > 10000 ||
            (Cell.IsReceiver() && (Cell.ReceiverWaterZ < -32768 || Cell.ReceiverWaterZ > 32768 ||
             Cell.GroundZ >= Cell.ReceiverWaterZ || (Cell.ReceiverId.High == 0 && Cell.ReceiverId.Low == 0))))
        {
            OutError = FString::Printf(TEXT("Shared drainage input is out of bounds or receiver is not genuinely wet and identified: cell=(%d,%d) ground=%d water=%d rain=%u receiver=%d id=(%llu,%llu)"),
                Cell.Cell.X, Cell.Cell.Y, Cell.GroundZ, Cell.ReceiverWaterZ, Cell.RainWeight,
                Cell.IsReceiver(), Cell.ReceiverId.High, Cell.ReceiverId.Low);
            return false;
        }
        if (Cell.bHasLocalOutlet && (Cell.IsReceiver() || Cell.LocalOutletBarrier < Cell.GroundZ ||
            Cell.LocalOutletBarrier < Cell.LocalOutletWaterZ || Cell.LocalOutletBarrier > 32768 ||
            Cell.LocalOutletWaterZ < -32768 || Cell.LocalOutletWaterZ > 32768 ||
            (Cell.LocalOutletId.High == 0 && Cell.LocalOutletId.Low == 0)))
        { OutError=TEXT("Invalid certified local runoff outlet"); return false; }
        Cell.bParentIsLocalOutlet=false;
        Cell.SpillZ = MAX_int32;
        Cell.PlateauDistance = MAX_int32;
        Cell.Parent = INDEX_NONE;
        Cell.bIncluded = false;
        return true;
    };
    if (Cells.IsEmpty())
    {
        FVoxelHydrologySharedCell Cell;
        const auto Read = InReadCell(Seed, Cell, OutError);
        if (Read == EVoxelHydrologyClosureRead::Pending) return EVoxelHydrologySharedSearchStep::NeedInput;
        if (Read != EVoxelHydrologyClosureRead::Ready) return Failure();
        Cell.Cell = Seed;
        if (!ValidateInput(Cell)) return Failure();
        int32 Index = INDEX_NONE;
        if (!Append(Cell, Cell.IsReceiver() ? Cell.ReceiverWaterZ : Cell.GroundZ, Index, OutError)) return Failure();
    }
    while (!bSearchComplete && InFaceBudget > 0)
    {
        if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return Failure(); }
        if (Current == INDEX_NONE)
        {
            if (Frontier.IsEmpty())
            {
                if (Threshold == MAX_int32)
                {
                    OutError = TEXT("Shared drainage world component contains no certified wet receiver");
                    return EVoxelHydrologySharedSearchStep::NoReceiver;
                }
                bSearchComplete = true;
                break;
            }
            if (Frontier[0].Plane > Threshold) { bSearchComplete = true; break; }
            const FEvent Event = Pop();
            Current = Event.Index;
            CurrentPlane = Event.Plane;
            Face = 0;
            Cells[Current].bIncluded = true;
            if (Cells[Current].bHasLocalOutlet)
                Threshold=FMath::Min(Threshold,FMath::Max(CurrentPlane,GetDetails(Cells[Current]).LocalOutletBarrier));
            if (Cells[Current].IsReceiver() || (bFineWaterOnly && Cells[Current].bHasCertifiedDryOutlet))
            {
                Threshold = FMath::Min(Threshold, CurrentPlane);
                Current = INDEX_NONE;
                --InFaceBudget;
                if (bFineWaterOnly)
                {
                    // 首真实湿根或已发布干列的真实排水路径给出精确种子水位。
                    // 所有低于该水位的种子连通成员及其邻接边界已读完；不扩展无关同高平台。
                    bSearchComplete = bReady = true;
                    return EVoxelHydrologySharedSearchStep::Ready;
                }
                continue;
            }
        }
        FIntPoint Position;
        if (Neighbor(Cells[Current].Cell, Face, Position) && !ByCell.Contains(Position))
        {
            FVoxelHydrologySharedCell Cell;
            const auto Read = InReadCell(Position, Cell, OutError);
            if (Read == EVoxelHydrologyClosureRead::Pending) return EVoxelHydrologySharedSearchStep::NeedInput;
            if (Read != EVoxelHydrologyClosureRead::Ready) return Failure();
            Cell.Cell = Position;
            if (!ValidateInput(Cell)) return Failure();
            const int32 Entry = FMath::Max(CurrentPlane, Cell.IsReceiver() ? Cell.ReceiverWaterZ : Cell.GroundZ);
            int32 Index = INDEX_NONE;
            if (!Append(Cell, Entry, Index, OutError)) return Failure();
        }
        --InFaceBudget;
        if (++Face == 4) Current = INDEX_NONE;
    }
    if (!bSearchComplete) return EVoxelHydrologySharedSearchStep::Yielded;
    const auto Solved = SolveCertificate(InFaceBudget, OutError, InCancel);
    if (Solved != EVoxelHydrologySharedSearchStep::Ready) return Solved;
    bReady = true;
    OutError.Reset();
    return EVoxelHydrologySharedSearchStep::Ready;
}

EVoxelHydrologySharedSearchStep FVoxelHydrologySharedSearch::SolveCertificate(int32 InWorkBudget,
    FString& OutError, const TAtomic<bool>* InCancel)
{
    auto Failure = [&]()
    {
        return OutError.StartsWith(TEXT("CapacityBlocked")) ?
            EVoxelHydrologySharedSearchStep::CapacityBlocked : EVoxelHydrologySharedSearchStep::Failed;
    };
    if (SolveStage == 0)
    {
        if (!VoxelGenerationAllocation::RetainArray(PlateauQueue, Cells.Num(), OutError)) return Failure();
        Frontier.Reset();
        PlateauQueue.Reset();
        SolveCursor = 0;
        SolveStage = 1;
    }
    while (InWorkBudget-- > 0)
    {
        if (InCancel && InCancel->Load())
        { OutError = TEXT("Canceled"); return EVoxelHydrologySharedSearchStep::Canceled; }
        if (SolveStage == 1)
        {
            if (SolveCursor == Cells.Num()) { SolveCursor = 0; SolveStage = 2; continue; }
            auto& Cell = Cells[SolveCursor];
            Cell.SpillZ = MAX_int32;
            Cell.PlateauDistance = MAX_int32;
            Cell.Parent = INDEX_NONE;
            Cell.bParentIsLocalOutlet=false;
            if (Cell.bIncluded && Cell.IsReceiver())
            {
                if (!Push(FEvent{SolveCursor, GetDetails(Cell).ReceiverWaterZ}, OutError)) return Failure();
                Cell.SpillZ = GetDetails(Cell).ReceiverWaterZ;
                Cell.PlateauDistance = 0;
            }
            else if (Cell.bIncluded && Cell.bHasLocalOutlet && GetDetails(Cell).LocalOutletBarrier<=Threshold)
            {
                if (!Push(FEvent{SolveCursor,GetDetails(Cell).LocalOutletBarrier},OutError)) return Failure();
                Cell.SpillZ=GetDetails(Cell).LocalOutletBarrier;
            }
            ++SolveCursor;
        }
        else if (SolveStage == 2)
        {
            if (SolveEvent.Index == INDEX_NONE)
            {
                if (Frontier.IsEmpty()) { SolveCursor = 0; SolveStage = 3; continue; }
                SolveEvent = Pop();
                if (SolveEvent.Plane!=Cells[SolveEvent.Index].SpillZ) { SolveEvent.Index=INDEX_NONE; continue; }
                SolveFace = 0;
            }
            FIntPoint Position;
            if (Neighbor(Cells[SolveEvent.Index].Cell, SolveFace, Position))
            {
                const int32* Found = ByCell.Find(Position);
                if (Found)
                {
                    auto& Target = Cells[*Found];
                    if (Target.bIncluded && !Target.IsReceiver())
                    {
                        const int32 Plane = FMath::Max(SolveEvent.Plane, Target.GroundZ);
                        if (Plane>=Target.SpillZ) { if (++SolveFace==4) SolveEvent.Index=INDEX_NONE; continue; }
                        if (!Push(FEvent{*Found, Plane}, OutError)) return Failure();
                        Target.SpillZ = Plane;
                    }
                }
            }
            if (++SolveFace == 4) SolveEvent.Index = INDEX_NONE;
        }
        else if (SolveStage == 3)
        {

            if (SolveCursor == Cells.Num()) { SolveCursor = 0; SolveStage = 4; continue; }
            auto& Cell = Cells[SolveCursor];
            if (Cell.bIncluded)
            {
                if (Cell.SpillZ == MAX_int32)
                { OutError = TEXT("Shared drainage certificate has an unreachable included cell"); return Failure(); }
                if (Cell.IsReceiver()) PlateauQueue.Add(SolveCursor);
                else if (Cell.bHasLocalOutlet && GetDetails(Cell).LocalOutletBarrier==Cell.SpillZ)
                {
                    Cell.bParentIsLocalOutlet=true; Cell.PlateauDistance=0; PlateauQueue.Add(SolveCursor);
                }
                else
                {
                    for (int32 Edge = 0; Edge < 4; ++Edge)
                    {
                        FIntPoint Position;
                        if (!Neighbor(Cell.Cell, Edge, Position)) continue;
                        const int32* Found = ByCell.Find(Position);
                        if (!Found || !Cells[*Found].bIncluded || Cells[*Found].SpillZ >= Cell.SpillZ) continue;
                        if (Cell.Parent == INDEX_NONE || SharedSearchCellLess(Position, Cells[Cell.Parent].Cell)) Cell.Parent = *Found;
                    }
                    if (Cell.Parent != INDEX_NONE) { Cell.PlateauDistance = 0; PlateauQueue.Add(SolveCursor); }
                }
            }
            ++SolveCursor;
        }
        else if (SolveStage == 4)
        {
            if (SolveCursor == PlateauQueue.Num()) { SolveCursor = 0; SolveStage = 5; continue; }
            const int32 Index = PlateauQueue[SolveCursor];
            for (int32 Edge = 0; Edge < 4; ++Edge)
            {
                FIntPoint Position;
                if (!Neighbor(Cells[Index].Cell, Edge, Position)) continue;
                const int32* Found = ByCell.Find(Position);
                if (!Found) continue;
                auto& Target = Cells[*Found];
                if (!Target.bIncluded || Target.SpillZ != Cells[Index].SpillZ || Target.PlateauDistance != MAX_int32) continue;
                if (Cells[Index].PlateauDistance == MAX_int32 - 1)
                { OutError = TEXT("Oversized: shared drainage plateau distance"); return Failure(); }
                Target.PlateauDistance = Cells[Index].PlateauDistance + 1;
                PlateauQueue.Add(*Found);
            }
            ++SolveCursor;
        }
        else if (SolveStage == 5)
        {
            if (SolveCursor == Cells.Num()) { SolveStage = 6; continue; }
            auto& Cell = Cells[SolveCursor];
            if (Cell.bIncluded && !Cell.IsReceiver() && !Cell.bParentIsLocalOutlet && Cell.Parent == INDEX_NONE)
            {
                if (Cell.PlateauDistance == MAX_int32)
                { OutError = TEXT("Shared drainage plateau has no descending anchor"); return Failure(); }
                for (int32 Edge = 0; Edge < 4; ++Edge)
                {
                    FIntPoint Position;
                    if (!Neighbor(Cell.Cell, Edge, Position)) continue;
                    const int32* Found = ByCell.Find(Position);
                    if (!Found) continue;
                    const auto& Target = Cells[*Found];
                    if (!Target.bIncluded || Target.SpillZ != Cell.SpillZ ||
                        int64(Target.PlateauDistance) + 1 != Cell.PlateauDistance) continue;
                    if (Cell.Parent == INDEX_NONE || SharedSearchCellLess(Position, Cells[Cell.Parent].Cell)) Cell.Parent = *Found;
                }
                if (Cell.Parent == INDEX_NONE)
                { OutError = TEXT("Shared drainage cell is missing its certified descending parent"); return Failure(); }
            }
            ++SolveCursor;
        }
        else return EVoxelHydrologySharedSearchStep::Ready;
    }
    return EVoxelHydrologySharedSearchStep::Yielded;
}

uint64 FVoxelHydrologySharedSearch::GetAllocatedBytes() const
{
    return Cells.GetAllocatedBytes() + Details.GetAllocatedBytes() +
        VoxelGenerationAllocation::AllocatedBytes(ByCell) +
        VoxelGenerationAllocation::AllocatedBytes(Frontier) +
        VoxelGenerationAllocation::AllocatedBytes(PlateauQueue);
}

void FVoxelHydrologySharedSearch::FreeBacking()
{
    Cells.FreeBacking();
    Details.FreeBacking();
    VoxelGenerationAllocation::FreeRetainedMap(ByCell);
    VoxelGenerationAllocation::FreeRetainedArray(Frontier);
    VoxelGenerationAllocation::FreeRetainedArray(PlateauQueue);
    SolveStage = 0;
    SolveCursor = 0;
    SolveFace = 0;
    SolveEvent = FEvent();
    MapCapacity = 0;
    Current = INDEX_NONE;
    Face = 0;
    Threshold = MAX_int32;
    bInitialized = false;
    bSearchComplete = false;
    bReady = false;
    bPreferDescendingWitness = false;
    bFineWaterOnly = false;
}
