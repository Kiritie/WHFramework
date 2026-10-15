#include "Voxel/Generation/Hydrology/VoxelHydrologyMacroInput.h"

#include "Voxel/Generation/VoxelGenerationMath.h"

bool FVoxelHydrologyRegionSample::IsValid() const
{
    return GroundZ >= -32768 && GroundZ <= 32768 && RainWeight <= 10000 &&
        (!IsReceiver() || (WaterZ > GroundZ && WaterZ <= 32768 && (WaterId.High != 0 || WaterId.Low != 0)));
}

uint64 FVoxelHydrologyRegionSample::AppendFingerprint(uint64 InFingerprint) const
{
    InFingerprint = VoxelGeneration::Mix(InFingerprint ^ uint64(uint32(GroundZ)));
    InFingerprint = VoxelGeneration::Mix(InFingerprint ^ RainWeight ^ uint64(uint32(WaterZ)));
    return VoxelGeneration::Mix(InFingerprint ^ WaterId.High ^ WaterId.Low);
}

bool FVoxelHydrologyMacroInput::IsCell(const FIntPoint& InCell, const int32 InCellSize)
{
    const int64 Minimum = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMin, InCellSize);
    const int64 Maximum = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMaxExclusive - 1, InCellSize);
    return InCell.X >= Minimum && InCell.Y >= Minimum && InCell.X <= Maximum && InCell.Y <= Maximum;
}

bool FVoxelHydrologyMacroInput::IsRegion(const FVoxelHydrologyRegionKey& InKey,
    const FVoxelGenerationSettings& InSettings)
{
    if (InSettings.HydrologyCellSize <= 0 || InSettings.HydrologyRegionSide <= 0)
    {
        return false;
    }
    const int64 Minimum = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMin, InSettings.HydrologyCellSize);
    const int64 Maximum = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMaxExclusive - 1, InSettings.HydrologyCellSize) + 1;
    const int64 StartX = int64(InKey.Coordinate.X) * InSettings.HydrologyRegionSide;
    const int64 StartY = int64(InKey.Coordinate.Y) * InSettings.HydrologyRegionSide;
    return StartX < Maximum && StartY < Maximum &&
        StartX + InSettings.HydrologyRegionSide > Minimum &&
        StartY + InSettings.HydrologyRegionSide > Minimum;
}

EVoxelHydrologyClosureRead FVoxelHydrologyMacroInput::Read(const FVoxelGenerationSettings& Settings,
    const FIntPoint& InCell, FTileLookup InLookup, FRequestTile InRequestTile,
    FVoxelHydrologyRegionSample& OutSample, FIntPoint& OutWorldPosition, FString& OutError)
{
    OutError.Reset();
    if (Settings.HydrologyCellSize <= 0 || Settings.HydrologyRegionSide <= 0 || Settings.HydrologyHaloCells < 0)
    {
        OutError = TEXT("Invalid hydrology macro input settings");
        return EVoxelHydrologyClosureRead::Failed;
    }
    if (!IsCell(InCell, Settings.HydrologyCellSize))
    {
        OutError = TEXT("Invalid hydrology component cell outside world domain");
        return EVoxelHydrologyClosureRead::Failed;
    }
    const FVoxelHydrologyRegionKey Owner{FIntPoint(
        VoxelGeneration::FloorDivide(InCell.X, Settings.HydrologyRegionSide),
        VoxelGeneration::FloorDivide(InCell.Y, Settings.HydrologyRegionSide))};
    const FVoxelHydrologyPlan* Tile = InLookup(Owner);
    if (!Tile)
    {
        return InRequestTile(Owner, OutError) ? EVoxelHydrologyClosureRead::Pending : EVoxelHydrologyClosureRead::Failed;
    }
    const int64 LocalX = int64(InCell.X) - Tile->Grid.WorldMinCell.X;
    const int64 LocalY = int64(InCell.Y) - Tile->Grid.WorldMinCell.Y;
    if (LocalX < 0 || LocalY < 0 || LocalX >= Tile->Grid.Width || LocalY >= Tile->Grid.Height)
    {
        OutError = TEXT("Canonical hydrology tile does not contain its owned cell");
        return EVoxelHydrologyClosureRead::Failed;
    }
    const uint32 Index = Tile->Grid.ToIndex(int32(LocalX), int32(LocalY));
    if (!Tile->Grid.GroundPlane.IsValidIndex(int32(Index)) ||
        !Tile->Grid.RainWeight.IsValidIndex(int32(Index)))
    {
        OutError = TEXT("Canonical hydrology tile has incomplete macro inputs");
        return EVoxelHydrologyClosureRead::Failed;
    }
    FVoxelHydrologyRegionSample Sample;
    FIntPoint WorldPosition;
    bool bReceiver = false;
    if (!Tile->Grid.TryToWorldVoxelXY(Index, WorldPosition))
    {
        OutError = TEXT("Canonical hydrology node has no world position");
        return EVoxelHydrologyClosureRead::Failed;
    }
    Sample.GroundZ = Tile->Grid.GroundPlane[Index];
    Sample.RainWeight = Tile->Grid.RainWeight[Index];
    if (const int32* Ocean = Tile->Grid.OceanOutlets.Find(Index))
    {
        bReceiver = true;
        Sample.WaterZ = *Ocean;
        Sample.WaterId = VoxelGeneration::MakeStableId(Settings.Seed,
            FIntVector(0, 0, Settings.SeaLevel), 0x4F4345414E524543ull, 0);
    }

    const int32 OwnerRadius = int32((int64(Settings.HydrologyHaloCells) +
        Settings.HydrologyRegionSide - 1) / Settings.HydrologyRegionSide);
    bool bMissing = false;
    bool bHasLakeReceiver = false;
    for (int32 OffsetY = -OwnerRadius; OffsetY <= OwnerRadius; ++OffsetY)
    {
        for (int32 OffsetX = -OwnerRadius; OffsetX <= OwnerRadius; ++OffsetX)
        {
            const int64 LakeOwnerX = int64(Owner.Coordinate.X) + OffsetX;
            const int64 LakeOwnerY = int64(Owner.Coordinate.Y) + OffsetY;
            if (LakeOwnerX < MIN_int32 || LakeOwnerY < MIN_int32 || LakeOwnerX > MAX_int32 || LakeOwnerY > MAX_int32)
            {
                continue;
            }
            const FVoxelHydrologyRegionKey LakeOwner{FIntPoint(int32(LakeOwnerX), int32(LakeOwnerY))};
            if (!IsRegion(LakeOwner, Settings))
            {
                continue;
            }
            const int64 LakeMinimumX = LakeOwnerX * Settings.HydrologyRegionSide - Settings.HydrologyHaloCells;
            const int64 LakeMinimumY = LakeOwnerY * Settings.HydrologyRegionSide - Settings.HydrologyHaloCells;
            const int64 LakeSide = int64(Settings.HydrologyRegionSide) + int64(Settings.HydrologyHaloCells) * 2;
            if (InCell.X < LakeMinimumX || InCell.X >= LakeMinimumX + LakeSide ||
                InCell.Y < LakeMinimumY || InCell.Y >= LakeMinimumY + LakeSide)
            {
                continue;
            }
            const FVoxelHydrologyPlan* LakeTile = InLookup(LakeOwner);
            if (!LakeTile)
            {
                if (!InRequestTile(LakeOwner, OutError))
                {
                    return EVoxelHydrologyClosureRead::Failed;
                }
                bMissing = true;
                continue;
            }
            if (const FVoxelLakePlan* Lake = LakeTile->FindLakeAtOrigin(WorldPosition))
            {
                if (!bReceiver || Lake->WaterZ > Sample.WaterZ ||
                    (Lake->WaterZ == Sample.WaterZ && (!bHasLakeReceiver || Lake->Id < Sample.WaterId)))
                {
                    Sample.WaterZ = Lake->WaterZ;
                    Sample.WaterId = Lake->Id;
                    bHasLakeReceiver = true;
                }
                bReceiver = true;
            }
        }
    }
    if (bMissing)
    {
        return EVoxelHydrologyClosureRead::Pending;
    }
    OutSample = Sample;
    OutWorldPosition = WorldPosition;
    OutError.Reset();
    return EVoxelHydrologyClosureRead::Ready;
}
