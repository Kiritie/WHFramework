#include "Voxel/Generation/Hydrology/VoxelHydrologyLakeCertificate.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "HAL/PlatformTime.h"
#include "Voxel/Generation/VoxelGenerationMath.h"

namespace
{
    int32 LakeCertificateCapacity(int32 Required, int32 Existing)
    { return int32(FMath::Min<int64>(MAX_int32, FMath::Max<int64>(Required, FMath::Max<int64>(16, int64(Existing) * 2)))); }

    void LakeSeenLocation(const FIntPoint& Cell, FIntPoint& OutTile, int32& OutBit)
    {
        OutTile = FIntPoint(VoxelGeneration::FloorDivide(Cell.X, 32), VoxelGeneration::FloorDivide(Cell.Y, 32));
        const int32 X = int32(int64(Cell.X) - int64(OutTile.X) * 32);
        const int32 Y = int32(int64(Cell.Y) - int64(OutTile.Y) * 32);
        check(X >= 0 && X < 32 && Y >= 0 && Y < 32);
        OutBit = X + Y * 32;
    }
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
    if (SeenCount == MAX_int32 || (bWet && Cells.Num() == MAX_int32))
    { OutError = TEXT("Oversized: fine lake certificate"); return false; }
    if (bWet && !Cells.ReserveOne(OutError)) return false;
    int32 ReceiverIndex = INDEX_NONE;
    if (bWet && Cell.IsReceiver())
    {
        const FReceiver Receiver{Cell.ReceiverWaterZ, Cell.ReceiverId};
        if (const int32* Existing = ReceiverIndices.Find(Receiver)) ReceiverIndex = *Existing;
        else
        {
            if (Receivers.Num() == MAX_int32)
            { OutError = TEXT("Oversized: fine lake receiver identities"); return false; }
            if (Receivers.Num() == ReceiverCapacity)
            {
                const int32 Capacity = LakeCertificateCapacity(ReceiverCapacity + 1, ReceiverCapacity);
                if (!VoxelGenerationAllocation::RetainArray(Receivers, Capacity, OutError) ||
                    !VoxelGenerationAllocation::RetainMap(ReceiverIndices, Capacity, OutError)) return false;
                ReceiverCapacity = Capacity;
            }
            ReceiverIndex = Receivers.Add(Receiver);
            ReceiverIndices.Add(Receiver, ReceiverIndex);
        }
    }
    if (!MarkSeen(Cell.Cell, OutError)) return false;
    if (bWet) Cells.Add({Cell.Cell, Cell.GroundZ, ReceiverIndex});
    return true;
}

bool FVoxelHydrologyLakeCertificate::HasSeen(const FIntPoint& InCell) const
{
    FIntPoint Tile;
    int32 Bit;
    LakeSeenLocation(InCell, Tile, Bit);
    const int32* Page = SeenTiles.Find(Tile);
    return Page && (SeenPages[*Page].Bits[Bit / 64] & (uint64(1) << (Bit % 64))) != 0;
}

bool FVoxelHydrologyLakeCertificate::MarkSeen(const FIntPoint& InCell, FString& OutError)
{
    FIntPoint Tile;
    int32 Bit;
    LakeSeenLocation(InCell, Tile, Bit);
    int32 PageIndex;
    if (const int32* Page = SeenTiles.Find(Tile)) PageIndex = *Page;
    else
    {
        if (SeenTiles.Num() == SeenTileCapacity)
        {
            if (SeenTileCapacity == MAX_int32)
            { OutError = TEXT("Oversized: fine lake visited tiles"); return false; }
            const int32 Capacity = LakeCertificateCapacity(SeenTileCapacity + 1, SeenTileCapacity);
            if (!VoxelGenerationAllocation::RetainMap(SeenTiles, Capacity, OutError)) return false;
            SeenTileCapacity = Capacity;
        }
        if (!SeenPages.ReserveOne(OutError)) return false;
        PageIndex = SeenPages.Add(FSeenTile());
        SeenTiles.Add(Tile, PageIndex);
    }
    uint64& Word = SeenPages[PageIndex].Bits[Bit / 64];
    const uint64 Mask = uint64(1) << (Bit % 64);
    if ((Word & Mask) == 0) { Word |= Mask; ++SeenCount; }
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
            if (!HasSeen(Position))
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
                // 所有已加入成员均通过严格低于水位的真实路径连通；更低受水体已足以否定这些根。
                // 不再遍历其余湖岸，也不认证尚未访问的坐标。接受闭合湖仍必须完成全部边界。
                if (bBelow && Value.IsReceiver() && Value.ReceiverWaterZ < WaterZ)
                {
                    bLowerContact = true;
                    bReady = true;
                    TraceState(true);
                    return EVoxelHydrologySharedSearchStep::Ready;
                }
                if (!Append(Value, bBelow && !bTerminal, OutError)) return Failure();
                if (bBelow && bSameBodyLevel && Value.ReceiverId < GroupId) GroupId = Value.ReceiverId;
            }
        }
        else
        {


            bOpenBoundary = true;
            bReady = true;
            TraceState(true);
            return EVoxelHydrologySharedSearchStep::Ready;
        }
        if (++Face == 4) { Face = 0; ++Cursor; }
    }
    if (Cursor < Cells.Num())
    {
        TraceState(false);
        return EVoxelHydrologySharedSearchStep::Yielded;
    }
    bReady = true;
    TraceState(true);
    return EVoxelHydrologySharedSearchStep::Ready;
}

void FVoxelHydrologyLakeCertificate::TraceState(bool bComplete)
{
    if (Cells.Num() < 32768) return;
    const double Now = FPlatformTime::Seconds();
    if (!bComplete && Now - LastDiagnosticSeconds < 5.0) return;
    LastDiagnosticSeconds = Now;
    UE_LOG(LogTemp, Display, TEXT("Voxel lake certificate: seed=(%d,%d) water=%d cells=%d seen=%d cursor=%d face=%d lower=%d open=%d ready=%d bytes=%llu"),
        Seed.Cell.X, Seed.Cell.Y, WaterZ, Cells.Num(), SeenCount, Cursor, Face,
        bLowerContact, bOpenBoundary, bReady, GetAllocatedBytes());
}

uint64 FVoxelHydrologyLakeCertificate::GetAllocatedBytes() const
{
    return Cells.GetAllocatedBytes() + SeenPages.GetAllocatedBytes() +
        VoxelGenerationAllocation::AllocatedBytes(SeenTiles) + VoxelGenerationAllocation::AllocatedBytes(Receivers) +
        VoxelGenerationAllocation::AllocatedBytes(ReceiverIndices);
}

FVoxelHydrologySharedCell FVoxelHydrologyLakeCertificate::FCellView::operator[](int32 InIndex) const
{
    const auto& Stored = Owner->Cells[InIndex];
    FVoxelHydrologySharedCell Result;
    Result.Cell = Stored.Cell;
    Result.GroundZ = Stored.GroundZ;
    if (Stored.ReceiverIndex != INDEX_NONE)
    {
        const auto& Receiver = Owner->Receivers[Stored.ReceiverIndex];
        Result.ReceiverWaterZ = Receiver.WaterZ;
        Result.ReceiverId = Receiver.Id;
    }
    return Result;
}

void FVoxelHydrologyLakeCertificate::FreeBacking()
{
    Cells.FreeBacking();
    SeenPages.FreeBacking();
    VoxelGenerationAllocation::FreeRetainedMap(SeenTiles);
    VoxelGenerationAllocation::FreeRetainedArray(Receivers);
    VoxelGenerationAllocation::FreeRetainedMap(ReceiverIndices);
    SeenCount = SeenTileCapacity = ReceiverCapacity = Cursor = Face = 0;
    WaterZ = MIN_int32;
    bInitialized = bReady = bLowerContact = bOpenBoundary = false;
    LastDiagnosticSeconds = 0.0;
}
