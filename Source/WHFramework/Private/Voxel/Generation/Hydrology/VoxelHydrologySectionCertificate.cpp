#include "Voxel/Generation/Hydrology/VoxelHydrologySectionCertificate.h"

bool FVoxelHydrologySectionCertificate::Initialize(const FIntPoint& InPosition,
    const FVoxelRiverShape& InShape, int32 InMinimumGroundZ, const FIntPoint& InWorldMinimum,
    const FIntPoint& InWorldMaximumInclusive, FString& OutError)
{
    if (bInitialized || InShape.BedHalfWidth < 1 || InShape.BedHalfWidth > InShape.WaterHalfWidth ||
        InShape.WaterHalfWidth > 256 || InShape.BankWidth < 1 || InShape.BankWidth > 512 ||
        InShape.ShoreWidth < 1 || InShape.ShoreWidth > 1024 || InShape.Depth < 1 || InShape.Depth > 256 ||
        InShape.MaxCutFill < 1 || InShape.MaxCutFill > 512 || InMinimumGroundZ < -32768 ||
        InMinimumGroundZ + InShape.Depth > 32766 ||
        InPosition.X < InWorldMinimum.X || InPosition.Y < InWorldMinimum.Y ||
        InPosition.X > InWorldMaximumInclusive.X || InPosition.Y > InWorldMaximumInclusive.Y)
    { OutError = TEXT("Invalid section certificate initialization"); return false; }
    Position = InPosition;
    Shape = InShape;
    WorldMinimum = InWorldMinimum;
    WorldMaximum = InWorldMaximumInclusive;
    MinimumWater = InMinimumGroundZ + Shape.Depth;
    Result.LowerWaterZ = MinimumWater;
    Result.UpperWaterZ = 32766;
    Radius = Shape.WaterHalfWidth + Shape.BankWidth + Shape.ShoreWidth;
    X = Y = -Radius;
    bInitialized = true;
    OutError.Reset();
    return true;
}

EVoxelHydrologySharedSearchStep FVoxelHydrologySectionCertificate::Advance(FReadSample Read,
    int32 WorkBudget, FString& OutError, const TAtomic<bool>* Cancel)
{
    OutError.Reset();
    if (Cancel && Cancel->Load()) { OutError = TEXT("Canceled"); return EVoxelHydrologySharedSearchStep::Canceled; }
    if (!bInitialized) { OutError = TEXT("Section certificate is not initialized"); return EVoxelHydrologySharedSearchStep::Failed; }
    if (bReady) return EVoxelHydrologySharedSearchStep::Ready;
    while (Y <= Radius && WorkBudget > 0)
    {
        --WorkBudget;
        if (Cancel && Cancel->Load()) { OutError = TEXT("Canceled"); return EVoxelHydrologySharedSearchStep::Canceled; }
        const int32 AX = FMath::Abs(X), AY = FMath::Abs(Y);
        const int32 Distance = FMath::Max(AX, AY) + FMath::Min(AX, AY) * 3 / 8;
        if (Distance <= Radius)
        {
            const int64 WX = int64(Position.X) + X, WY = int64(Position.Y) + Y;
            if (WX < WorldMinimum.X || WY < WorldMinimum.Y || WX > WorldMaximum.X || WY > WorldMaximum.Y)
            {


                bOpenFootprint = true;
            }
            else
            {
                const FIntPoint Cell{int32(WX), int32(WY)};
                FVoxelHydrologySectionSample Sample;
                const auto Status = Read(Cell, Sample, OutError);
                if (Status == EVoxelHydrologyClosureRead::Pending) return OutError == TEXT("Canceled") ? EVoxelHydrologySharedSearchStep::Canceled : OutError.StartsWith(TEXT("Yielded")) ? EVoxelHydrologySharedSearchStep::Yielded : OutError.StartsWith(TEXT("CapacityBlocked")) ? EVoxelHydrologySharedSearchStep::CapacityBlocked : EVoxelHydrologySharedSearchStep::NeedInput;
                if (Status != EVoxelHydrologyClosureRead::Ready)
                    return OutError == TEXT("Canceled") ? EVoxelHydrologySharedSearchStep::Canceled : OutError.StartsWith(TEXT("Yielded")) ? EVoxelHydrologySharedSearchStep::Yielded : OutError.StartsWith(TEXT("CapacityBlocked")) ? EVoxelHydrologySharedSearchStep::CapacityBlocked : EVoxelHydrologySharedSearchStep::Failed;
                if (Sample.OriginalGround < -32768 || Sample.OriginalGround > 32768 ||
                    (Sample.ExistingWaterZ != MIN_int32 && (Sample.ExistingWaterZ < -32768 ||
                     Sample.ExistingWaterZ > 32768 || Sample.OriginalGround >= Sample.ExistingWaterZ ||
                     (Sample.ExistingBodyId.High == 0 && Sample.ExistingBodyId.Low == 0))))
                { OutError = TEXT("Invalid original terrain or receiving-water section sample"); return EVoxelHydrologySharedSearchStep::Failed; }
                auto AddContact = [&](const FIntPoint& Contact, const FVoxelHydrologySectionSample& Wet)
                {
                    if (Result.FixedWaterZ != MIN_int32 && Result.FixedWaterZ != Wet.ExistingWaterZ)
                        bConflictingWater = true;
                    if (Result.FixedWaterZ == MIN_int32 || Wet.ExistingBodyId < Result.FixedBodyId ||
                        (Wet.ExistingBodyId == Result.FixedBodyId &&
                         (Contact.X < Result.FixedConnector.X || (Contact.X == Result.FixedConnector.X && Contact.Y < Result.FixedConnector.Y))))
                    {
                        Result.FixedBodyId = Wet.ExistingBodyId;
                        Result.FixedConnector = Contact;
                    }
                    Result.FixedWaterZ = Wet.ExistingWaterZ;
                };
                if (Sample.ExistingWaterZ != MIN_int32)
                {


                    if (Distance <= Shape.WaterHalfWidth) AddContact(Cell, Sample);
                }
                else
                {
                    int32 RequiredGround = Sample.OriginalGround - Shape.MaxCutFill;
                    static const FIntPoint Directions[4] = { FIntPoint(1,0), FIntPoint(-1,0), FIntPoint(0,1), FIntPoint(0,-1) };
                    for (const FIntPoint& Direction : Directions)
                    {
                        const int64 NX = WX + Direction.X, NY = WY + Direction.Y;
                        if (NX < WorldMinimum.X || NY < WorldMinimum.Y || NX > WorldMaximum.X || NY > WorldMaximum.Y)
                        { bOpenFootprint = true; continue; }
                        const FIntPoint Neighbour{int32(NX), int32(NY)};
                        FVoxelHydrologySectionSample Adjacent;
                        const auto AdjacentStatus = Read(Neighbour, Adjacent, OutError);
                        if (AdjacentStatus == EVoxelHydrologyClosureRead::Pending) return OutError == TEXT("Canceled") ? EVoxelHydrologySharedSearchStep::Canceled : OutError.StartsWith(TEXT("Yielded")) ? EVoxelHydrologySharedSearchStep::Yielded : OutError.StartsWith(TEXT("CapacityBlocked")) ? EVoxelHydrologySharedSearchStep::CapacityBlocked : EVoxelHydrologySharedSearchStep::NeedInput;
                        if (AdjacentStatus != EVoxelHydrologyClosureRead::Ready)
                            return OutError == TEXT("Canceled") ? EVoxelHydrologySharedSearchStep::Canceled : OutError.StartsWith(TEXT("Yielded")) ? EVoxelHydrologySharedSearchStep::Yielded : OutError.StartsWith(TEXT("CapacityBlocked")) ? EVoxelHydrologySharedSearchStep::CapacityBlocked : EVoxelHydrologySharedSearchStep::Failed;
                        if (Adjacent.OriginalGround < -32768 || Adjacent.OriginalGround > 32768 ||
                            (Adjacent.ExistingWaterZ != MIN_int32 && (Adjacent.ExistingWaterZ < -32768 ||
                             Adjacent.ExistingWaterZ > 32768 || Adjacent.OriginalGround >= Adjacent.ExistingWaterZ ||
                             (Adjacent.ExistingBodyId.High == 0 && Adjacent.ExistingBodyId.Low == 0))))
                        { OutError = TEXT("Invalid adjacent receiving-water sample"); return EVoxelHydrologySharedSearchStep::Failed; }
                        if (Adjacent.ExistingWaterZ == MIN_int32) continue;


                        if (Distance <= Shape.WaterHalfWidth) AddContact(Neighbour, Adjacent);
                        else RequiredGround = FMath::Max(RequiredGround, Adjacent.ExistingWaterZ);
                    }
                    if (Distance <= Shape.WaterHalfWidth + Shape.BankWidth)
                        Result.UpperWaterZ = FMath::Min(Result.UpperWaterZ, Sample.OriginalGround);


                    auto GroundAt = [&](int32 Water, int32& Ground)
                    {
                        FVoxelRiverSection Section;
                        if (!VoxelHydrology::EvaluateRiverSection(Sample.OriginalGround, Water, Distance, Shape, Section, OutError)) return false;
                        Ground = FMath::Min(Sample.OriginalGround, Section.GroundPlane);
                        return true;
                    };
                    int32 Low = MinimumWater, High = 32766, Ground = 0;
                    if (!GroundAt(High, Ground)) return EVoxelHydrologySharedSearchStep::Failed;
                    if (Ground < RequiredGround) bConflictingWater = true;
                    else
                    {
                        while (Low < High)
                        {
                            const int32 Mid = Low + (High - Low) / 2;
                            if (!GroundAt(Mid, Ground)) return EVoxelHydrologySharedSearchStep::Failed;
                            if (Ground >= RequiredGround) High = Mid;
                            else Low = Mid + 1;
                        }
                        Result.LowerWaterZ = FMath::Max(Result.LowerWaterZ, Low);
                    }
                }
            }
        }
        if (++X > Radius) { X = -Radius; ++Y; }
    }
    if (Y <= Radius) return EVoxelHydrologySharedSearchStep::Yielded;
    if (Result.FixedWaterZ != MIN_int32)
    {
        Result.LowerWaterZ = FMath::Max(Result.LowerWaterZ, Result.FixedWaterZ);
        Result.UpperWaterZ = FMath::Min(Result.UpperWaterZ, Result.FixedWaterZ);
    }
    Result.bEligible = !bOpenFootprint && !bConflictingWater && Result.LowerWaterZ <= Result.UpperWaterZ;
    bReady = true;
    return EVoxelHydrologySharedSearchStep::Ready;
}

void FVoxelHydrologySectionCertificate::Reset()
{
    Result = FVoxelHydrologySectionInterval();
    bInitialized = bReady = bConflictingWater = bOpenFootprint = false;
    Radius = X = Y = MinimumWater = 0;
}
