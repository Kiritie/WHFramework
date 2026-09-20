#include "Voxel/Generation/Aquifer/VoxelAquiferGenerator.h"
#include "Voxel/Generation/VoxelGenerationMath.h"

FVoxelAquiferGenerator::FVoxelAquiferGenerator(TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe)
    : Recipe(InRecipe)
{
}

FVoxelAquiferSample FVoxelAquiferGenerator::Sample(
    const FIntVector& InCell,
    const FVoxelColumnSample& InColumn,
    int32 InDensityQ16) const
{
    FVoxelAquiferSample Result;

    if (InDensityQ16 > 0)
    {
        return Result;
    }

    const FVoxelGenerationSettings& Settings = Recipe->Settings;

    if (InCell.Z <= Settings.LavaCeiling)
    {
        Result.Fluid = EVoxelFluidKind::Lava;
        Result.SurfaceZ = Settings.LavaCeiling;
        return Result;
    }

    const int32 RegionX = VoxelGeneration::FloorDivide(
        InCell.X,
        Settings.AquiferSpacing);

    const int32 RegionY = VoxelGeneration::FloorDivide(
        InCell.Y,
        Settings.AquiferSpacing);

    const int32 RegionZ = VoxelGeneration::FloorDivide(
        InCell.Z,
        Settings.AquiferSpacing);

    const FIntVector Region(RegionX, RegionY, RegionZ);

    const uint64 Seed = VoxelGeneration::MakeSeed(
        Settings.Seed,
        Region,
        0xA9F129B7D41C2E63ull);

    FIntVector Center;
    Center.X = RegionX * Settings.AquiferSpacing +
        VoxelGeneration::RandomRange(
            Seed,
            Settings.AquiferSpacing / 4,
            Settings.AquiferSpacing * 3 / 4);

    Center.Y = RegionY * Settings.AquiferSpacing +
        VoxelGeneration::RandomRange(
            VoxelGeneration::Mix(Seed),
            Settings.AquiferSpacing / 4,
            Settings.AquiferSpacing * 3 / 4);

    Center.Z = RegionZ * Settings.AquiferSpacing +
        VoxelGeneration::RandomRange(
            VoxelGeneration::Mix(Seed ^ 0xBE12C07Bull),
            Settings.AquiferSpacing / 4,
            Settings.AquiferSpacing * 3 / 4);

    const FIntVector Radius(
        Settings.AquiferRadius,
        Settings.AquiferRadius,
        FMath::Max(4, Settings.AquiferRadius / 2));

    if (!VoxelGeneration::IsInsideEllipsoid(
        InCell,
        Center,
        Radius))
    {
        return Result;
    }

    const int32 LocalWaterZ = FMath::Min(
        InColumn.SurfaceZ - 8,
        Center.Z + Radius.Z / 2);

    if (InCell.Z <= LocalWaterZ)
    {
        Result.Fluid = EVoxelFluidKind::Water;
        Result.SurfaceZ = LocalWaterZ;
    }

    return Result;
}
