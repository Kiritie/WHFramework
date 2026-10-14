#include "Voxel/Generation/Hydrology/VoxelHydrologyLakeCertificate.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

namespace
{
    int32 LakeCertificateCapacity(int32 Required, int32 Existing)
    { return int32(FMath::Min<int64>(MAX_int32, FMath::Max<int64>(Required, FMath::Max<int64>(16, int64(Existing) * 2)))); }
}

bool FVoxelHydrologyLakeCertificate::Initialize(const FVoxelHydrologySharedCell& InSeed,
    const FVoxelStableId& InOceanId, const FIntPoint& InMinimum, const FIntPoint& InMaximum, FString& OutError)
{
    if (bInitialized || !InSeed.IsReceiver() || InSeed.GroundZ < -32768 || InSeed.GroundZ > 32768 ||
        InSeed.ReceiverWaterZ < -32768 || InSeed.ReceiverWaterZ > 32768 ||
        (InSeed.ReceiverId.High == 0 && InSeed.ReceiverId.Low == 0) ||
        InSeed.GroundZ >= InSeed.ReceiverWaterZ ||
        InSeed.Cell.X < InMinimum.X || InSeed.Cell.Y < InMinimum.Y ||
        InSeed.Cell.X > InMaximum.X || InSeed.Cell.Y > InMaximum.Y)
    { OutError = TEXT("Invalid fine lake certificate initialization"); return false; }
    Seed = InSeed;
    WaterZ = Seed.ReceiverWaterZ;
    GroupId = Seed.ReceiverId;
    OceanId = InOceanId;
    Minimum = InMinimum;
    Maximum = InMaximum;
    bInitialized = true;
    OutError.Reset();
    return true;
}

bool FVoxelHydrologyLakeCertificate::Append(const FVoxelHydrologySharedCell& Cell,
    bool bWet, FString& OutError)
{
    if (Seen.Num() == MAX_int32 || (bWet && Cells.Num() == MAX_int32))
    { OutError = TEXT("Oversized: fine lake certificate"); return false; }
    if (bWet && Cells.Num() == Cells.Max() && !VoxelGenerationAllocation::RetainArray(Cells,
        LakeCertificateCapacity(Cells.Num() + 1, Cells.Max()), OutError)) return false;
    if (Seen.Num() == SeenCapacity)
    {
        const int32 Capacity = LakeCertificateCapacity(SeenCapacity + 1, SeenCapacity);
        if (!VoxelGenerationAllocation::RetainSet(Seen, Capacity, OutError)) return false;
        SeenCapacity = Capacity;
    }
    Seen.Add(Cell.Cell);
    if (bWet) Cells.Add(Cell);
    return true;
}

EVoxelHydrologySharedSearchStep FVoxelHydrologyLakeCertificate::Advance(FReadCell InRead,
    int32 InBudget, FString& OutError, const TAtomic<bool>* InCancel)
{
    auto Failure = [&]()
    {
        return OutError.StartsWith(TEXT("CapacityBlocked")) ? EVoxelHydrologySharedSearchStep::CapacityBlocked :
            EVoxelHydrologySharedSearchStep::Failed;
    };
    OutError.Reset();
    if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return EVoxelHydrologySharedSearchStep::Canceled; }
    if (bReady) return EVoxelHydrologySharedSearchStep::Ready;
    if (!bInitialized) { OutError = TEXT("Fine lake certificate is not initialized"); return Failure(); }
    if (InBudget <= 0) return EVoxelHydrologySharedSearchStep::Yielded;
    if (Cells.IsEmpty() && !Append(Seed, true, OutError)) return Failure();
    static const FIntPoint Offsets[] = {FIntPoint(-1, 0), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(0, 1)};
    while (Cursor < Cells.Num() && InBudget-- > 0)
    {
        if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return EVoxelHydrologySharedSearchStep::Canceled; }
        const int64 X = int64(Cells[Cursor].Cell.X) + Offsets[Face].X;
        const int64 Y = int64(Cells[Cursor].Cell.Y) + Offsets[Face].Y;
        if (X >= Minimum.X && Y >= Minimum.Y && X <= Maximum.X && Y <= Maximum.Y)
        {
            const FIntPoint Position{int32(X), int32(Y)};
            if (!Seen.Contains(Position))
            {
                FVoxelHydrologySharedCell Value;
                const auto Read = InRead(Position, Value, OutError);
                if (Read == EVoxelHydrologyClosureRead::Pending) return EVoxelHydrologySharedSearchStep::NeedInput;
                if (Read != EVoxelHydrologyClosureRead::Ready) return Failure();
                Value.Cell = Position;
                if (Value.GroundZ < -32768 || Value.GroundZ > 32768 ||
                    (Value.IsReceiver() && (Value.ReceiverWaterZ < -32768 || Value.ReceiverWaterZ > 32768 ||
                        Value.GroundZ >= Value.ReceiverWaterZ)))
                { OutError = TEXT("Invalid fine lake certificate input"); return Failure(); }
                const bool bBelow = Value.GroundZ < WaterZ;
                const bool bSameBodyLevel = Value.IsReceiver() && Value.ReceiverWaterZ == WaterZ;

                const bool bTerminal = Value.IsReceiver() &&
                    (Value.ReceiverId == OceanId || Value.ReceiverWaterZ < WaterZ);
                if (!Append(Value, bBelow && !bTerminal, OutError)) return Failure();
                if (bBelow && Value.IsReceiver() && Value.ReceiverWaterZ < WaterZ) bLowerContact = true;
                if (bBelow && bSameBodyLevel && Value.ReceiverId < GroupId) GroupId = Value.ReceiverId;
            }
        }
        else
        {


            bOpenBoundary = true;
        }
        if (++Face == 4) { Face = 0; ++Cursor; }
    }
    if (Cursor < Cells.Num()) return EVoxelHydrologySharedSearchStep::Yielded;
    bReady = true;
    return EVoxelHydrologySharedSearchStep::Ready;
}

uint64 FVoxelHydrologyLakeCertificate::GetAllocatedBytes() const
{ return VoxelGenerationAllocation::AllocatedBytes(Cells) + VoxelGenerationAllocation::AllocatedBytes(Seen); }

void FVoxelHydrologyLakeCertificate::FreeBacking()
{
    VoxelGenerationAllocation::FreeRetainedArray(Cells);
    VoxelGenerationAllocation::FreeRetainedSet(Seen);
    SeenCapacity = Cursor = Face = 0;
    WaterZ = MIN_int32;
    bInitialized = bReady = bLowerContact = bOpenBoundary = false;
}
