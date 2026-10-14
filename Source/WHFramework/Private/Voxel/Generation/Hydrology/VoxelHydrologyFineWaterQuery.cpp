#include "Voxel/Generation/Hydrology/VoxelHydrologyFineWaterQuery.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

namespace
{
    int32 FineWaterCapacity(int32 Required, int32 Previous)
    {
        return int32(FMath::Min<int64>(MAX_int32,
            FMath::Max<int64>(Required, FMath::Max<int64>(16, int64(Previous) * 2))));
    }
    bool FineWaterLess(const FIntPoint& A, const FIntPoint& B)
    { return A.X != B.X ? A.X < B.X : A.Y < B.Y; }
    EVoxelHydrologyFineWaterStep FineWaterFailure(const FString& Error)
    {
        if (Error == TEXT("Canceled")) return EVoxelHydrologyFineWaterStep::Canceled;
        if (Error.StartsWith(TEXT("CapacityBlocked"))) return EVoxelHydrologyFineWaterStep::CapacityBlocked;
        return EVoxelHydrologyFineWaterStep::Failed;
    }
    EVoxelHydrologyFineWaterStep FineWaterConvert(EVoxelHydrologySharedSearchStep Step, const FString& Error)
    {
        switch (Step)
        {
        case EVoxelHydrologySharedSearchStep::NeedInput:

            return Error.StartsWith(TEXT("Yielded")) ? EVoxelHydrologyFineWaterStep::Yielded : EVoxelHydrologyFineWaterStep::NeedTiles;
        case EVoxelHydrologySharedSearchStep::CapacityBlocked: return EVoxelHydrologyFineWaterStep::CapacityBlocked;
        case EVoxelHydrologySharedSearchStep::Ready: return EVoxelHydrologyFineWaterStep::Ready;
        case EVoxelHydrologySharedSearchStep::NoReceiver: return EVoxelHydrologyFineWaterStep::NoReceiver;
        case EVoxelHydrologySharedSearchStep::Failed: return EVoxelHydrologyFineWaterStep::Failed;
        case EVoxelHydrologySharedSearchStep::Canceled: return EVoxelHydrologyFineWaterStep::Canceled;
        default: return EVoxelHydrologyFineWaterStep::Yielded;
        }
    }
}

bool FVoxelHydrologyFineWaterQuery::Initialize(const FIntPoint& Seed, const FIntPoint& Minimum,
    const FIntPoint& MaximumInclusive, FString& OutError)
{
    if (bInitialized) { OutError = TEXT("Fine water query already initialized"); return false; }
    if (!Search.Initialize(Seed, Minimum, MaximumInclusive, OutError)) return false;
    WorldMinimum = Minimum;
    WorldMaximum = MaximumInclusive;
    bInitialized = true;
    return true;
}

bool FVoxelHydrologyFineWaterQuery::AddMember(const FIntPoint& Position, int32 CertificateIndex, FString& OutError)
{
    if (Members.Num() == MAX_int32) { OutError = TEXT("Oversized: complete fine natural water domain"); return false; }
    if (Members.Num() == Members.Max() && !VoxelGenerationAllocation::RetainArray(Members,
        FineWaterCapacity(Members.Num() + 1, Members.Max()), OutError)) return false;
    Members.Add(Position);
    Visited[CertificateIndex] = 1;
    if (Members.Num() == 1 || FineWaterLess(Position, Result.NaturalKey.MinimumMember)) Result.NaturalKey.MinimumMember = Position;
    return true;
}

EVoxelHydrologyFineWaterStep FVoxelHydrologyFineWaterQuery::Advance(FReadCell Reader,
    int32 WorkBudget, FString& OutError, const TAtomic<bool>* Cancel)
{
    OutError.Reset();
    if (!bInitialized) { OutError = TEXT("Fine water query not initialized"); return FineWaterFailure(OutError); }
    if (Cancel && Cancel->Load()) { OutError = TEXT("Canceled"); return EVoxelHydrologyFineWaterStep::Canceled; }
    if (bReady) return EVoxelHydrologyFineWaterStep::Ready;
    while (WorkBudget > 0)
    {
        --WorkBudget;
        if (Cancel && Cancel->Load()) { OutError = TEXT("Canceled"); return EVoxelHydrologyFineWaterStep::Canceled; }
        if (Stage == 0)
        {


            const auto Step = Search.Advance(Reader, 1, OutError, Cancel);
            if (Step == EVoxelHydrologySharedSearchStep::Yielded) continue;
            if (Step != EVoxelHydrologySharedSearchStep::Ready) return FineWaterConvert(Step, OutError);
            const auto& Seed = Search.GetSeedResult();
            if (Seed.IsReceiver())
            {
                Result.bFixedWater = true;
                Result.WaterZ = Seed.ReceiverWaterZ;
                Result.FixedId = Seed.ReceiverId;
                bReady = true;
                return EVoxelHydrologyFineWaterStep::Ready;
            }
            if (Seed.GroundZ >= Seed.SpillZ)
            {
                bReady = true;
                return EVoxelHydrologyFineWaterStep::Ready;
            }
            Result.bNaturalWater = true;
            Result.WaterZ = Seed.SpillZ;
            Result.NaturalKey.WaterZ = Seed.SpillZ;
            Cursor = 0;
            Stage = 1;
        }
        else if (Stage == 1)
        {
            const auto& Cells = Search.GetCells();
            if (Cursor == Cells.Num())
            {
                if (!AddMember(Cells[0].Cell, 0, OutError)) return FineWaterFailure(OutError);
                Cursor = 0;
                Face = 0;
                Stage = 2;
                continue;
            }
            if (CertificateByCell.Num() == MapCapacity)
            {
                const int32 Capacity = FineWaterCapacity(MapCapacity + 1, MapCapacity);
                if (!VoxelGenerationAllocation::RetainMap(CertificateByCell, Capacity, OutError)) return FineWaterFailure(OutError);
                MapCapacity = Capacity;
            }
            if (Visited.Num() == Visited.Max() && !VoxelGenerationAllocation::RetainArray(Visited,
                FineWaterCapacity(Visited.Num() + 1, Visited.Max()), OutError)) return FineWaterFailure(OutError);
            CertificateByCell.Add(Cells[Cursor].Cell, Cursor);
            Visited.Add(0);
            ++Cursor;
        }
        else if (Stage == 2)
        {
            if (Cursor == Members.Num()) { Cursor = 0; Stage = 3; continue; }
            static const FIntPoint Offsets[] = {FIntPoint(-1, 0), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(0, 1)};
            const int64 X = int64(Members[Cursor].X) + Offsets[Face].X;
            const int64 Y = int64(Members[Cursor].Y) + Offsets[Face].Y;
            if (X >= WorldMinimum.X && X <= WorldMaximum.X && Y >= WorldMinimum.Y && Y <= WorldMaximum.Y)
            {
                const FIntPoint Position{int32(X), int32(Y)};
                const int32* Found = CertificateByCell.Find(Position);
                if (!Found)
                { OutError = TEXT("Fine water certificate lacks a physical-domain member boundary"); return FineWaterFailure(OutError); }
                const auto& Cell = Search.GetCells()[*Found];
                if (!Visited[*Found])
                {
                    if (Cell.IsReceiver())
                    {
                        if (FixedContacts.Num() == MAX_int32) { OutError = TEXT("Oversized: fine water fixed boundary"); return FineWaterFailure(OutError); }
                        if (FixedContacts.Num() == FixedContacts.Max() && !VoxelGenerationAllocation::RetainArray(FixedContacts,
                            FineWaterCapacity(FixedContacts.Num() + 1, FixedContacts.Max()), OutError)) return FineWaterFailure(OutError);
                        FixedContacts.Add(Cell);
                        Visited[*Found] = 2;
                    }
                    else if (Cell.bIncluded && Cell.SpillZ == Result.WaterZ && Cell.GroundZ < Result.WaterZ)
                    {
                        if (!AddMember(Position, *Found, OutError)) return FineWaterFailure(OutError);
                    }
                }
            }
            if (++Face == 4) { Face = 0; ++Cursor; }
        }
        else if (Stage == 3)
        {

            if (SortBuffer.Max() < Members.Num() && !VoxelGenerationAllocation::RetainArray(SortBuffer,
                Members.Num(), OutError)) return FineWaterFailure(OutError);
            if (SortBuffer.Num() < Members.Num()) { SortBuffer.Add(FIntPoint::ZeroValue); continue; }
            Stage = 4;
        }
        else if (Stage == 4)
        {
            if (SortWidth >= Members.Num())
            {
                if (Cancel && Cancel->Load()) { OutError = TEXT("Canceled"); return EVoxelHydrologyFineWaterStep::Canceled; }
                bReady = true;
                return EVoxelHydrologyFineWaterStep::Ready;
            }
            if (bSortCopy)
            {
                if (Cursor < Members.Num()) { Members[Cursor] = SortBuffer[Cursor]; ++Cursor; continue; }
                SortWidth *= 2;
                SortRun = 0;
                bSortCopy = false;
                continue;
            }
            if (SortRun >= Members.Num()) { Cursor = 0; bSortCopy = true; continue; }
            const int64 Mid = FMath::Min<int64>(SortRun + SortWidth, Members.Num());
            const int64 End = FMath::Min<int64>(SortRun + SortWidth * 2, Members.Num());
            if (!bMergeStarted)
            {
                SortLeft = SortRun; SortRight = Mid; SortWrite = SortRun;
                bMergeStarted = true;
            }
            if (SortWrite == End) { SortRun = End; bMergeStarted = false; continue; }
            if (SortLeft < Mid && (SortRight == End || FineWaterLess(Members[int32(SortLeft)], Members[int32(SortRight)])))
                SortBuffer[int32(SortWrite++)] = Members[int32(SortLeft++)];
            else SortBuffer[int32(SortWrite++)] = Members[int32(SortRight++)];
        }
    }
    return EVoxelHydrologyFineWaterStep::Yielded;
}

int32 FVoxelHydrologyFineWaterQuery::GetMemberGround(int32 MemberIndex) const
{
    check(bReady && Members.IsValidIndex(MemberIndex));
    return Search.GetCells()[CertificateByCell.FindChecked(Members[MemberIndex])].GroundZ;
}

uint64 FVoxelHydrologyFineWaterQuery::GetAllocatedBytes() const
{
    return Search.GetAllocatedBytes() + VoxelGenerationAllocation::AllocatedBytes(CertificateByCell) +
        VoxelGenerationAllocation::AllocatedBytes(Visited) + VoxelGenerationAllocation::AllocatedBytes(Members) +
        VoxelGenerationAllocation::AllocatedBytes(SortBuffer) + VoxelGenerationAllocation::AllocatedBytes(FixedContacts);
}

void FVoxelHydrologyFineWaterQuery::FreeBacking()
{
    Search.FreeBacking();
    VoxelGenerationAllocation::FreeRetainedMap(CertificateByCell);
    VoxelGenerationAllocation::FreeRetainedArray(Visited);
    VoxelGenerationAllocation::FreeRetainedArray(Members);
    VoxelGenerationAllocation::FreeRetainedArray(SortBuffer);
    VoxelGenerationAllocation::FreeRetainedArray(FixedContacts);
    Result = FVoxelHydrologyFineWaterResult();
    MapCapacity = Cursor = Face = 0;
    SortWidth = 1;
    SortRun = SortLeft = SortRight = SortWrite = 0;
    bMergeStarted = bSortCopy = false;
    Stage = 0;
    bInitialized = bReady = false;
}

bool FVoxelHydrologyFineWaterCoverage::Initialize(const FIntPoint& CoverageMinimum,
    const FIntPoint& CoverageMaximumInclusive, const FIntPoint& WorldMinimum,
    const FIntPoint& WorldMaximumInclusive, FString& OutError)
{
    if (bInitialized || CoverageMinimum.X > CoverageMaximumInclusive.X || CoverageMinimum.Y > CoverageMaximumInclusive.Y ||
        CoverageMinimum.X < WorldMinimum.X || CoverageMinimum.Y < WorldMinimum.Y ||
        CoverageMaximumInclusive.X > WorldMaximumInclusive.X || CoverageMaximumInclusive.Y > WorldMaximumInclusive.Y)
    { OutError = TEXT("Invalid fine water coverage or nonphysical world bounds"); return false; }
    Minimum = CoverageMinimum; Maximum = CoverageMaximumInclusive;
    WorldMin = WorldMinimum; WorldMax = WorldMaximumInclusive; Current = Minimum;
    bInitialized = true;
    OutError.Reset();
    return true;
}

EVoxelHydrologyFineWaterStep FVoxelHydrologyFineWaterCoverage::Advance(FVoxelHydrologyFineWaterQuery::FReadCell Reader,
    int32 WorkBudget, FString& OutError, const TAtomic<bool>* Cancel)
{
    OutError.Reset();
    if (!bInitialized) { OutError = TEXT("Fine water coverage not initialized"); return FineWaterFailure(OutError); }
    if (Cancel && Cancel->Load()) { OutError = TEXT("Canceled"); return EVoxelHydrologyFineWaterStep::Canceled; }
    if (bReady) return EVoxelHydrologyFineWaterStep::Ready;
    while (WorkBudget > 0)
    {
        --WorkBudget;
        if (Cancel && Cancel->Load()) { OutError = TEXT("Canceled"); return EVoxelHydrologyFineWaterStep::Canceled; }
        if (!bQueryStarted && !Samples.Contains(Current))
        {
            if (!Query.Initialize(Current, WorldMin, WorldMax, OutError)) return FineWaterFailure(OutError);
            bQueryStarted = true;
            PublishCursor = 0;
        }
        if (bQueryStarted)
        {
            const auto Step = Query.Advance(Reader, 1, OutError, Cancel);
            if (Step == EVoxelHydrologyFineWaterStep::Yielded) continue;
            if (Step != EVoxelHydrologyFineWaterStep::Ready) return Step;
            const auto& Cells = Query.GetCells();
            const int32 Count = Cells.IsEmpty() ? 1 : Cells.Num();
            if (PublishCursor < Count)
            {
                const FIntPoint Position = Cells.IsEmpty() ? Current : Cells[PublishCursor];
                if (!Samples.Contains(Position))
                {
                    if (Samples.Num() == MAX_int32) { OutError = TEXT("Oversized: fine coverage cache"); return FineWaterFailure(OutError); }
                    if (Samples.Num() == MapCapacity)
                    {
                        const int32 Capacity = FineWaterCapacity(MapCapacity + 1, MapCapacity);
                        if (!VoxelGenerationAllocation::RetainMap(Samples, Capacity, OutError)) return FineWaterFailure(OutError);
                        MapCapacity = Capacity;
                    }
                    Samples.Add(Position, Query.GetResult());
                }
                ++PublishCursor;
                continue;
            }
            Query.FreeBacking();
            bQueryStarted = false;
        }
        if (Current.X == Maximum.X)
        {
            if (Current.Y == Maximum.Y) { bReady = true; return EVoxelHydrologyFineWaterStep::Ready; }
            Current.X = Minimum.X;
            ++Current.Y;
        }
        else ++Current.X;
    }
    return EVoxelHydrologyFineWaterStep::Yielded;
}

const FVoxelHydrologyFineWaterResult* FVoxelHydrologyFineWaterCoverage::Find(const FIntPoint& Position) const
{
    if (!bReady || Position.X < Minimum.X || Position.Y < Minimum.Y || Position.X > Maximum.X || Position.Y > Maximum.Y) return nullptr;
    return Samples.Find(Position);
}

uint64 FVoxelHydrologyFineWaterCoverage::GetAllocatedBytes() const
{ return Query.GetAllocatedBytes() + VoxelGenerationAllocation::AllocatedBytes(Samples); }

void FVoxelHydrologyFineWaterCoverage::FreeBacking()
{
    Query.FreeBacking();
    VoxelGenerationAllocation::FreeRetainedMap(Samples);
    MapCapacity = PublishCursor = 0;
    bQueryStarted = bInitialized = bReady = false;
}
