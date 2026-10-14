#include "Voxel/Generation/Hydrology/VoxelRiverCorridor.h"

#include "Voxel/Generation/Terrain/VoxelTerrainGenerator.h"
#include "Voxel/Generation/VoxelGenerationMath.h"

bool VoxelHydrology::ProjectRiverSegment(const FVoxelRiverMeanderPoint& InA,
    const FVoxelRiverMeanderPoint& InB, const FIntPoint& InSample,
    FVoxelRiverProjection& OutProjection)
{
    auto IsValidShape = [](const FVoxelRiverMeanderPoint& InPoint)
    {
        return InPoint.BedHalfWidth >= 1 && InPoint.BedHalfWidth <= InPoint.WaterHalfWidth &&
            InPoint.WaterHalfWidth <= 256 && InPoint.BankWidth >= 1 && InPoint.BankWidth <= 512 &&
            InPoint.ShoreWidth >= 1 && InPoint.ShoreWidth <= 1024 &&
            InPoint.Depth >= 1 && InPoint.Depth <= 256;
    };
    if (!IsValidShape(InA) || !IsValidShape(InB))
    {
        return false;
    }
    const int64 SegmentX = int64(InB.Position.X) - InA.Position.X;
    const int64 SegmentY = int64(InB.Position.Y) - InA.Position.Y;
    const int64 SampleX = int64(InSample.X) - InA.Position.X;
    const int64 SampleY = int64(InSample.Y) - InA.Position.Y;
    const double LengthSquared = double(SegmentX) * SegmentX + double(SegmentY) * SegmentY;
    if (LengthSquared <= 0.0)
    {
        return false;
    }

    const double Alpha = FMath::Clamp(
        (double(SampleX) * SegmentX + double(SampleY) * SegmentY) / LengthSquared, 0.0, 1.0);
    const int32 AlphaQ16 = int32(Alpha * 65536.0);
    const double DistanceX = FMath::Abs(double(SampleX) - double(SegmentX) * Alpha);
    const double DistanceY = FMath::Abs(double(SampleY) - double(SegmentY) * Alpha);
    auto Blend = [AlphaQ16](const int32 InFirst, const int32 InSecond)
    {
        return int32(int64(InFirst) + (int64(InSecond) - InFirst) * AlphaQ16 / 65536);
    };

    FVoxelRiverProjection Projection;
    Projection.WaterPlane = Blend(InA.WaterZ, InB.WaterZ);
    Projection.DistanceCells = int32(FMath::Min(double(MAX_int32),
        FMath::Max(DistanceX, DistanceY) + FMath::Min(DistanceX, DistanceY) * 3.0 / 8.0));
    Projection.Shape.BedHalfWidth = Blend(InA.BedHalfWidth, InB.BedHalfWidth);
    Projection.Shape.WaterHalfWidth = Blend(InA.WaterHalfWidth, InB.WaterHalfWidth);
    Projection.Shape.Depth = Blend(InA.Depth, InB.Depth);
    Projection.Shape.bWalkableMargins = InA.bWalkableMargins && InB.bWalkableMargins;
    Projection.Shape.BankWidth = Blend(InA.BankWidth, InB.BankWidth);
    Projection.Shape.ShoreWidth = Blend(InA.ShoreWidth, InB.ShoreWidth);
    const int32 Curvature = AlphaQ16 < 32768 ? InA.Curvature : InB.Curvature;
    const double Side = double(SegmentX) * SampleY - double(SegmentY) * SampleX;
    if (Curvature != 0 && Side != 0.0)
    {
        if ((Curvature > 0 && Side < 0.0) || (Curvature < 0 && Side > 0.0))
        {
            const int32 Shift = FMath::Min(Projection.Shape.ShoreWidth - 1,
                FMath::Max(1, Projection.Shape.BankWidth / 5));
            Projection.Shape.BankWidth += Shift;
            Projection.Shape.ShoreWidth -= Shift;
        }
        else
        {
            const int32 Shift = FMath::Min(Projection.Shape.BankWidth - 1,
                FMath::Max(1, Projection.Shape.BankWidth / 10));
            Projection.Shape.BankWidth -= Shift;
            Projection.Shape.ShoreWidth += Shift;
        }
    }
    Projection.Shape.MaxCutFill = FMath::Max(64, Projection.Shape.Depth * 4);
    OutProjection = Projection;
    return true;
}

bool VoxelHydrology::SampleRiverCorridorCap(const FVoxelRiverMeanderPoint& InA,
    const FVoxelRiverMeanderPoint& InB, const FVoxelTerrainGenerator& InTerrain,
    int32& OutUpperWaterPlane, FString& OutError, const TAtomic<bool>* InCancel)
{
    FIntPoint LowestCell = FIntPoint::ZeroValue;
    return SampleRiverCorridorCap(InA, InB, InTerrain,
        [](const FIntPoint&) { return false; }, OutUpperWaterPlane, LowestCell, OutError, InCancel);
}

bool VoxelHydrology::SampleRiverCorridorCap(const FVoxelRiverMeanderPoint& InA,
    const FVoxelRiverMeanderPoint& InB, const FVoxelTerrainGenerator& InTerrain,
    TFunctionRef<bool(const FIntPoint&)> InIsReceivingWater,
    int32& OutUpperWaterPlane, FIntPoint& OutLowestCell,
    FString& OutError, const TAtomic<bool>* InCancel)
{
    auto IsValidPoint = [](const FVoxelRiverMeanderPoint& InPoint)
    {
        return VoxelGeneration::IsWorldCell(InPoint.Position.X) &&
            VoxelGeneration::IsWorldCell(InPoint.Position.Y) &&
            InPoint.BedHalfWidth >= 1 && InPoint.BedHalfWidth <= InPoint.WaterHalfWidth &&
            InPoint.WaterHalfWidth <= 256 && InPoint.BankWidth >= 1 && InPoint.BankWidth <= 512 &&
            InPoint.ShoreWidth >= 1 && InPoint.ShoreWidth <= 1024 &&
            InPoint.Depth >= 1 && InPoint.Depth <= 256;
    };
    if (!IsValidPoint(InA) || !IsValidPoint(InB) || InA.Position == InB.Position)
    {
        OutError = TEXT("Invalid river corridor segment");
        return false;
    }

    const int32 MaximumWetWidth = FMath::Max(InA.WaterHalfWidth, InB.WaterHalfWidth);
    const int32 MaximumBankWidth = FMath::Max(InA.BankWidth, InB.BankWidth);
    const int64 Radius = int64(MaximumWetWidth) + MaximumBankWidth +
        FMath::Max(1, MaximumBankWidth / 5) + 1;
    const int64 MinimumX = FMath::Max(VoxelGeneration::WorldCellMin,
        int64(FMath::Min(InA.Position.X, InB.Position.X)) - Radius);
    const int64 MinimumY = FMath::Max(VoxelGeneration::WorldCellMin,
        int64(FMath::Min(InA.Position.Y, InB.Position.Y)) - Radius);
    const int64 MaximumX = FMath::Min(VoxelGeneration::WorldCellMaxExclusive - 1,
        int64(FMath::Max(InA.Position.X, InB.Position.X)) + Radius);
    const int64 MaximumY = FMath::Min(VoxelGeneration::WorldCellMaxExclusive - 1,
        int64(FMath::Max(InA.Position.Y, InB.Position.Y)) + Radius);

    int32 UpperWaterPlane = MAX_int32;
    FIntPoint LowestCell = FIntPoint::ZeroValue;
    bool bHasColumn = false;
    uint32 CancelCounter = 0;
    for (int64 WorldY = MinimumY; WorldY <= MaximumY; ++WorldY)
    {
        int64 RowMinimumX = MinimumX;
        int64 RowMaximumX = MaximumX;
        const int64 SegmentY = int64(InB.Position.Y) - InA.Position.Y;
        if (SegmentY != 0)
        {
            const double FirstAlpha = (double(WorldY - Radius) - InA.Position.Y) / double(SegmentY);
            const double LastAlpha = (double(WorldY + Radius) - InA.Position.Y) / double(SegmentY);
            const double MinimumAlpha = FMath::Clamp(FMath::Min(FirstAlpha, LastAlpha), 0.0, 1.0);
            const double MaximumAlpha = FMath::Clamp(FMath::Max(FirstAlpha, LastAlpha), 0.0, 1.0);
            const double SegmentX = double(int64(InB.Position.X) - InA.Position.X);
            const double FirstX = double(InA.Position.X) + SegmentX * MinimumAlpha;
            const double LastX = double(InA.Position.X) + SegmentX * MaximumAlpha;
            RowMinimumX = FMath::Max(MinimumX, int64(FMath::FloorToDouble(FMath::Min(FirstX, LastX))) - Radius - 1);
            RowMaximumX = FMath::Min(MaximumX, int64(FMath::CeilToDouble(FMath::Max(FirstX, LastX))) + Radius + 1);
        }
        for (int64 WorldX = RowMinimumX; WorldX <= RowMaximumX; ++WorldX)
        {
            if ((CancelCounter++ & 255u) == 0u && InCancel && InCancel->Load())
            {
                OutError = TEXT("Canceled");
                return false;
            }
            FVoxelRiverProjection Projection;
            if (!ProjectRiverSegment(InA, InB, FIntPoint(int32(WorldX), int32(WorldY)), Projection) ||
                Projection.DistanceCells > Projection.Shape.WaterHalfWidth + Projection.Shape.BankWidth)
            {
                continue;
            }
            bHasColumn = true;
            const FIntPoint Cell{int32(WorldX), int32(WorldY)};
            if (InIsReceivingWater(Cell))
            {
                continue;
            }
            const int32 Ground = InTerrain.SampleSurfaceHeight(Cell.X, Cell.Y);
            if (Ground < UpperWaterPlane)
            {
                UpperWaterPlane = Ground;
                LowestCell = Cell;
            }
        }
    }
    if (!bHasColumn)
    {
        OutError = TEXT("River corridor contains no terrain samples");
        return false;
    }
    if (InCancel && InCancel->Load())
    {
        OutError = TEXT("Canceled");
        return false;
    }
    OutUpperWaterPlane = FMath::Min(UpperWaterPlane, 32766);
    OutLowestCell = LowestCell;
    OutError.Reset();
    return true;
}
