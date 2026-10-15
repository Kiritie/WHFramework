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
    if (!Search.Initialize(Seed, Minimum, MaximumInclusive, OutError,
        EVoxelHydrologySharedSearchPurpose::FineWaterBody)) return false;
    if (!DryWitness.Initialize(Seed, Minimum, MaximumInclusive, OutError)) return false;
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
    if (MemberGrounds.Num() == MemberGrounds.Max() && !VoxelGenerationAllocation::RetainArray(MemberGrounds,
        FineWaterCapacity(MemberGrounds.Num() + 1, MemberGrounds.Max()), OutError)) return false;
    Members.Add(Position);
    MemberGrounds.Add(Search.GetCells()[CertificateIndex].GroundZ);
    Visited[CertificateIndex] = 1;
    if (Members.Num() == 1 || FineWaterLess(Position, Result.NaturalKey.MinimumMember)) Result.NaturalKey.MinimumMember = Position;
    return true;
}

bool FVoxelHydrologyFineWaterQuery::AddCertificate(const FVoxelHydrologySharedCell& InCell, FString& OutError)
{
    if (Certificates.Num() == MAX_int32)
    {
        OutError = TEXT("Oversized: fine water certificates");
        return false;
    }
    if (Certificates.Num() == Certificates.Max() && !VoxelGenerationAllocation::RetainArray(Certificates,
        FineWaterCapacity(Certificates.Num() + 1, Certificates.Max()), OutError)) return false;
    FVoxelHydrologyFineWaterCertificate& Certificate = Certificates.AddDefaulted_GetRef();
    Certificate.Cell = InCell.Cell;
    Certificate.GroundZ = InCell.GroundZ;
    Certificate.SpillZ = InCell.SpillZ;
    Certificate.ReceiverWaterZ = InCell.ReceiverWaterZ;
    Certificate.ReceiverId = InCell.ReceiverId;
    Certificate.bIncluded = InCell.bIncluded;
    return true;
}

EVoxelHydrologyFineWaterStep FVoxelHydrologyFineWaterQuery::Advance(FReadCell Reader,
    int32 WorkBudget, FString& OutError, const TAtomic<bool>* Cancel, const FWetHint* WetHint)
{
    OutError.Reset();
    if (!bInitialized) { OutError = TEXT("Fine water query not initialized"); return FineWaterFailure(OutError); }
    if (Cancel && Cancel->Load()) { OutError = TEXT("Canceled"); return EVoxelHydrologyFineWaterStep::Canceled; }
    if (bReady) return EVoxelHydrologyFineWaterStep::Ready;
    if (!bWitnessFinished && WetHint)
    {
        const auto Step = DryWitness.Advance(Reader, *WetHint, WorkBudget, OutError, Cancel);
        if (Step == EVoxelHydrologyDryWitnessStep::Proven)
        {
            if (!AddCertificate(DryWitness.GetCertificate(), OutError)) return FineWaterFailure(OutError);
            SeedGround = DryWitness.GetCertificate().GroundZ;
            DryWitness.FreeBacking();
            Search.FreeBacking();
            bReady = true;
            return EVoxelHydrologyFineWaterStep::Ready;
        }
        if (Step == EVoxelHydrologyDryWitnessStep::Unproven)
        {
            DryWitness.FreeBacking();
            bWitnessFinished = true;
            return EVoxelHydrologyFineWaterStep::Yielded;
        }
        if (Step == EVoxelHydrologyDryWitnessStep::Yielded) return EVoxelHydrologyFineWaterStep::Yielded;
        if (Step == EVoxelHydrologyDryWitnessStep::NeedInput)
        {
            return OutError.StartsWith(TEXT("Yielded")) ? EVoxelHydrologyFineWaterStep::Yielded : EVoxelHydrologyFineWaterStep::NeedTiles;
        }
        return FineWaterFailure(OutError);
    }
    bWitnessFinished = true;
    while (WorkBudget > 0)
    {
        --WorkBudget;
        if (Cancel && Cancel->Load()) { OutError = TEXT("Canceled"); return EVoxelHydrologyFineWaterStep::Canceled; }
        if (Stage == 0)
        {


            const auto Step = Search.Advance(Reader, 1, OutError, Cancel);
            if (Cancel && Cancel->Load())
            {
                OutError = TEXT("Canceled");
                return EVoxelHydrologyFineWaterStep::Canceled;
            }
            FVoxelHydrologySharedCell DryCell;
            if ((Step == EVoxelHydrologySharedSearchStep::Yielded || Step == EVoxelHydrologySharedSearchStep::Ready) &&
                Search.TryGetSeedDryCertificate(DryCell))
            {
                if (!AddCertificate(DryCell, OutError)) return FineWaterFailure(OutError);
                SeedGround = DryCell.GroundZ;
                Search.FreeBacking();
                DryWitness.FreeBacking();
                bReady = true;
                return EVoxelHydrologyFineWaterStep::Ready;
            }
            if (Step == EVoxelHydrologySharedSearchStep::Yielded) continue;
            if (Step != EVoxelHydrologySharedSearchStep::Ready) return FineWaterConvert(Step, OutError);
            const auto& Seed = Search.GetSeedResult();
            SeedGround = Seed.GroundZ;
            if (Seed.IsReceiver())
            {
                Result.bFixedWater = true;
                Result.WaterZ = Seed.ReceiverWaterZ;
                Result.FixedId = Seed.ReceiverId;
                Cursor = 0;
                Stage = 5;
                continue;
            }
            if (Seed.GroundZ >= Seed.SpillZ)
            {
                Cursor = 0;
                Stage = 5;
                continue;
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
            if (Visited.Num() == Visited.Max() && !VoxelGenerationAllocation::RetainArray(Visited,
                FineWaterCapacity(Visited.Num() + 1, Visited.Max()), OutError)) return FineWaterFailure(OutError);
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
                const int32 Found = Search.FindCellIndex(Position);
                if (Found == INDEX_NONE)
                { OutError = TEXT("Fine water certificate lacks a physical-domain member boundary"); return FineWaterFailure(OutError); }
                const auto& Cell = Search.GetCells()[Found];
                if (!Visited[Found])
                {
                    if (Cell.IsReceiver())
                    {
                        if (FixedContacts.Num() == MAX_int32) { OutError = TEXT("Oversized: fine water fixed boundary"); return FineWaterFailure(OutError); }
                        if (FixedContacts.Num() == FixedContacts.Max() && !VoxelGenerationAllocation::RetainArray(FixedContacts,
                            FineWaterCapacity(FixedContacts.Num() + 1, FixedContacts.Max()), OutError)) return FineWaterFailure(OutError);
                        FixedContacts.Add(Cell);
                        Visited[Found] = 2;
                    }
                    // 首湿根已证明种子最小水位；通过严格低于水位的四邻接路径，
                    // 与种子连通的每个非接收列都有相同精确水位，完整遍历到实体边界。
                    else if (Cell.bIncluded && Cell.GroundZ < Result.WaterZ)
                    {
                        if (!AddMember(Position, Found, OutError)) return FineWaterFailure(OutError);
                    }
                }
            }
            if (++Face == 4) { Face = 0; ++Cursor; }
        }
        else if (Stage == 3)
        {

            if (SortBuffer.Max() < Members.Num() && !VoxelGenerationAllocation::RetainArray(SortBuffer,
                Members.Num(), OutError)) return FineWaterFailure(OutError);
            if (GroundSortBuffer.Max() < Members.Num() && !VoxelGenerationAllocation::RetainArray(GroundSortBuffer,
                Members.Num(), OutError)) return FineWaterFailure(OutError);
            if (SortBuffer.Num() < Members.Num())
            {
                SortBuffer.Add(FIntPoint::ZeroValue);
                GroundSortBuffer.Add(0);
                continue;
            }
            Stage = 4;
        }
        else if (Stage == 4)
        {
            if (SortWidth >= Members.Num())
            {
                if (Cancel && Cancel->Load()) { OutError = TEXT("Canceled"); return EVoxelHydrologyFineWaterStep::Canceled; }
                Cursor = 0;
                Stage = 5;
                continue;
            }
            if (bSortCopy)
            {
                if (Cursor < Members.Num())
                {
                    Members[Cursor] = SortBuffer[Cursor];
                    MemberGrounds[Cursor] = GroundSortBuffer[Cursor];
                    ++Cursor;
                    continue;
                }
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
            const int32 Source = SortLeft < Mid && (SortRight == End || FineWaterLess(Members[int32(SortLeft)], Members[int32(SortRight)]))
                ? int32(SortLeft++) : int32(SortRight++);
            const int32 Destination = int32(SortWrite++);
            SortBuffer[Destination] = Members[Source];
            GroundSortBuffer[Destination] = MemberGrounds[Source];
        }
        else if (Stage == 5)
        {
            if (Cursor < Search.GetCells().Num())
            {
                const auto& Cell = Search.GetCells()[Cursor];
                if (Cell.bIncluded && (Cell.IsReceiver() || Cell.GroundZ >= Cell.SpillZ))
                {
                    if (!AddCertificate(Cell, OutError)) return FineWaterFailure(OutError);
                }
                ++Cursor;
                continue;
            }
            Search.FreeBacking();
            DryWitness.FreeBacking();
            VoxelGenerationAllocation::FreeRetainedArray(Visited);
            VoxelGenerationAllocation::FreeRetainedArray(SortBuffer);
            VoxelGenerationAllocation::FreeRetainedArray(GroundSortBuffer);
            bReady = true;
            return EVoxelHydrologyFineWaterStep::Ready;
        }
    }
    return EVoxelHydrologyFineWaterStep::Yielded;
}

int32 FVoxelHydrologyFineWaterQuery::GetMemberGround(int32 MemberIndex) const
{
    check(bReady && Members.IsValidIndex(MemberIndex));
    return MemberGrounds[MemberIndex];
}

uint64 FVoxelHydrologyFineWaterQuery::GetAllocatedBytes() const
{
    return Search.GetAllocatedBytes() + DryWitness.GetAllocatedBytes() + VoxelGenerationAllocation::AllocatedBytes(Certificates) +
        VoxelGenerationAllocation::AllocatedBytes(Visited) + VoxelGenerationAllocation::AllocatedBytes(Members) +
        VoxelGenerationAllocation::AllocatedBytes(MemberGrounds) + VoxelGenerationAllocation::AllocatedBytes(SortBuffer) +
        VoxelGenerationAllocation::AllocatedBytes(GroundSortBuffer) + VoxelGenerationAllocation::AllocatedBytes(FixedContacts);
}

void FVoxelHydrologyFineWaterQuery::FreeBacking()
{
    Search.FreeBacking();
    DryWitness.FreeBacking();
    VoxelGenerationAllocation::FreeRetainedArray(Certificates);
    VoxelGenerationAllocation::FreeRetainedArray(Visited);
    VoxelGenerationAllocation::FreeRetainedArray(Members);
    VoxelGenerationAllocation::FreeRetainedArray(MemberGrounds);
    VoxelGenerationAllocation::FreeRetainedArray(SortBuffer);
    VoxelGenerationAllocation::FreeRetainedArray(GroundSortBuffer);
    VoxelGenerationAllocation::FreeRetainedArray(FixedContacts);
    Result = FVoxelHydrologyFineWaterResult();
    SeedGround = Cursor = Face = 0;
    SortWidth = 1;
    SortRun = SortLeft = SortRight = SortWrite = 0;
    bMergeStarted = bSortCopy = false;
    Stage = 0;
    bInitialized = bReady = false;
    bWitnessFinished = false;
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
