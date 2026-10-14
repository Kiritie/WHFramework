#include "Voxel/Generation/Hydrology/VoxelHydrologyBasinClosure.h"

#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationMath.h"

namespace
{
    EVoxelHydrologyClosureStatus AllocationStatus(const FString& InError)
    {
        if (InError == TEXT("Canceled")) return EVoxelHydrologyClosureStatus::Canceled;
        return InError.StartsWith(TEXT("CapacityBlocked")) ?
            EVoxelHydrologyClosureStatus::CapacityBlocked : EVoxelHydrologyClosureStatus::Failed;
    }

    int32 ClosureCapacity(const int32 InRequired, const int32 InCurrent)
    {
        return int32(FMath::Min<int64>(MAX_int32,
            FMath::Max<int64>(InRequired, FMath::Max<int64>(16, int64(InCurrent) * 2))));
    }
}

bool FVoxelHydrologyBasinClosure::Initialize(const FIntPoint& InSeed,
    const int32 InWaterPlane, FString& OutError)
{
    if (InWaterPlane < -32768 || InWaterPlane > 32767 ||
        bInitialized || Cells.Num() || Seen.Num() ||
        !VoxelGeneration::IsWorldCell(InSeed.X) || !VoxelGeneration::IsWorldCell(InSeed.Y))
    {
        OutError = TEXT("Invalid basin closure initialization");
        return false;
    }
    Seed = InSeed;
    WaterPlane = InWaterPlane;
    bInitialized = true;
    OutError.Reset();
    return true;
}

FVoxelHydrologyClosureStep FVoxelHydrologyBasinClosure::Stop(
    const EVoxelHydrologyClosureStatus InStatus, const FIntPoint& InCell,
    const FIntPoint& InFrom, const FString& InDiagnostic) const
{
    FVoxelHydrologyClosureStep Result;
    Result.Status = InStatus;
    Result.RequestedCell = InCell;
    Result.FromCell = InFrom;
    Result.Diagnostic = InDiagnostic;
    return Result;
}

bool FVoxelHydrologyBasinClosure::AppendCell(const FIntPoint& InCell,
    const int32 InMaxWaterCells, FString& OutError)
{
    if (Cells.Num() >= InMaxWaterCells || Cells.Num() == MAX_int32)
    {
        OutError = TEXT("CapacityBlocked: basin closure requires a larger retained cell allowance");
        return false;
    }
    const int32 Required = Cells.Num() + 1;


    if (Required > Cells.Max() && !VoxelGenerationAllocation::RetainArray(
        Cells, ClosureCapacity(Required, Cells.Max()), OutError)) return false;
    if (Required > SeenCapacity)
    {
        const int32 Capacity = ClosureCapacity(Required, SeenCapacity);
        if (!VoxelGenerationAllocation::RetainSet(Seen, Capacity, OutError)) return false;
        SeenCapacity = Capacity;
    }
    Cells.Add(InCell);
    Seen.Add(InCell);
    return true;
}

FVoxelHydrologyClosureStep FVoxelHydrologyBasinClosure::Advance(FReadGround InReadGround,
    FReadConnector InReadConnector, const int32 InFaceBudget, const int32 InMaxWaterCells,
    const TAtomic<bool>* InCancel)
{
    if (InCancel && InCancel->Load())
        return Stop(EVoxelHydrologyClosureStatus::Canceled, Seed, Seed, TEXT("Canceled"));
    if (bReady) return Stop(EVoxelHydrologyClosureStatus::Ready, Seed, Seed, FString());
    if (bConflict) return Stop(EVoxelHydrologyClosureStatus::Conflict, Seed, Seed,
        TEXT("Basin closure is conflicted; its owner must reroute or reclassify before restarting"));
    if (!bInitialized || InFaceBudget < 1 || InMaxWaterCells < 1)
        return Stop(EVoxelHydrologyClosureStatus::Failed, Seed, Seed, TEXT("Invalid basin closure arguments"));
    FString Error;
    const auto* Allocation = FVoxelGenerationAllocationScope::Get();
    if (!(Allocation ? Allocation->Check(Error) : FVoxelGenerationAllocationScope::AllowPassive(Error)))
        return Stop(AllocationStatus(Error), Seed, Seed, Error);

    auto Conflict = [this](const FIntPoint& At, const FIntPoint& From, const FString& Why)
    {
        bConflict = true;
        return Stop(EVoxelHydrologyClosureStatus::Conflict, At, From, Why);
    };
    if (Cells.IsEmpty())
    {
        int32 Ground = 0;
        const auto Read = InReadGround(Seed, Ground, Error);
        if (Read == EVoxelHydrologyClosureRead::Pending)
            return Stop(EVoxelHydrologyClosureStatus::NeedTerrain, Seed, Seed, Error);
        if (Read == EVoxelHydrologyClosureRead::Failed)
            return Stop(EVoxelHydrologyClosureStatus::Failed, Seed, Seed, Error);
        if (Ground >= WaterPlane)
            return Conflict(Seed, Seed, TEXT("Dry seed cannot certify a receiving-water expansion"));
        if (!AppendCell(Seed, InMaxWaterCells, Error))
            return Stop(AllocationStatus(Error), Seed, Seed, Error);
    }


    static const FIntPoint Directions[4] = {
        FIntPoint(-1, 0), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(0, 1)};
    int32 FacesProcessed = 0;
    while (Cursor < Cells.Num() && FacesProcessed < InFaceBudget)
    {
        if (InCancel && InCancel->Load())
            return Stop(EVoxelHydrologyClosureStatus::Canceled, Cells[Cursor], Cells[Cursor], TEXT("Canceled"));
        const FIntPoint From = Cells[Cursor];
        const FInt64Vector2 ToWide(int64(From.X) + Directions[Face].X,
            int64(From.Y) + Directions[Face].Y);
        FIntPoint To;
        const bool bInside = VoxelGeneration::TryWorldPoint(ToWide, To);
        if (bInside && Seen.Contains(To))
        {

        }
        else
        {
            TOptional<int32> ConnectorPlane;
            Error.Reset();
            const auto Connector = InReadConnector(From, ToWide, ConnectorPlane, Error);
            if (Connector == EVoxelHydrologyClosureRead::Pending)
                return Stop(EVoxelHydrologyClosureStatus::NeedConnector, bInside ? To : From, From, Error);
            if (Connector == EVoxelHydrologyClosureRead::Failed)
                return Stop(EVoxelHydrologyClosureStatus::Failed, bInside ? To : From, From, Error);
            if (ConnectorPlane.IsSet())
            {
                if (ConnectorPlane.GetValue() != WaterPlane)
                    return Conflict(bInside ? To : From, From,
                        TEXT("Receiving-water connection has a different fixed plane; reroute or reclassify upstream"));
                ++ConnectorEdges;
            }
            else
            {
                if (!bInside)
                    return Conflict(From, From, TEXT("World edge has no certified receiving-water connector"));
                int32 Ground = 0;
                Error.Reset();
                const auto Read = InReadGround(To, Ground, Error);
                if (Read == EVoxelHydrologyClosureRead::Pending)
                    return Stop(EVoxelHydrologyClosureStatus::NeedTerrain, To, From, Error);
                if (Read == EVoxelHydrologyClosureRead::Failed)
                    return Stop(EVoxelHydrologyClosureStatus::Failed, To, From, Error);
                if (Ground >= WaterPlane)
                {
                    ++ClosedEdges;
                }
                else if (!AppendCell(To, InMaxWaterCells, Error))
                {
                    return Stop(AllocationStatus(Error), To, From, Error);
                }
            }
        }
        ++FacesProcessed;
        if (++Face == 4) { Face = 0; ++Cursor; }
    }
    if (Cursor != Cells.Num())
        return Stop(EVoxelHydrologyClosureStatus::Yielded, Cells[Cursor], Cells[Cursor], FString());
    if (ClosedEdges > MAX_uint32 || ConnectorEdges > MAX_uint32)
        return Stop(EVoxelHydrologyClosureStatus::Failed, Seed, Seed,
            TEXT("Oversized: basin certificate edge counters exceed native range"));
    Certificate.WaterCells = uint32(Cells.Num());
    Certificate.ClosedEdges = uint32(ClosedEdges);
    Certificate.ConnectorEdges = uint32(ConnectorEdges);
    bReady = true;
    return Stop(EVoxelHydrologyClosureStatus::Ready, Seed, Seed, FString());
}

uint64 FVoxelHydrologyBasinClosure::GetAllocatedBytes() const
{
    return VoxelGenerationAllocation::AllocatedBytes(Cells) +
        VoxelGenerationAllocation::AllocatedBytes(Seen);
}

void FVoxelHydrologyBasinClosure::FreeBacking()
{
    VoxelGenerationAllocation::FreeRetainedArray(Cells);
    VoxelGenerationAllocation::FreeRetainedSet(Seen);
    SeenCapacity = 0;
    Cursor = 0;
    Face = 0;
    WaterPlane = 0;
    Seed = FIntPoint::ZeroValue;
    ClosedEdges = 0;
    ConnectorEdges = 0;
    Certificate = FVoxelBasinCertificate();
    bInitialized = false;
    bReady = false;
    bConflict = false;
}
