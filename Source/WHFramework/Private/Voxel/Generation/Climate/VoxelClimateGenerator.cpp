#include "Voxel/Generation/Climate/VoxelClimateGenerator.h"
#include "Voxel/Generation/VoxelGenerationMath.h"

FVoxelClimateGenerator::FVoxelClimateGenerator(TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe)
    : Recipe(InRecipe)
{
}

FVoxelClimateSample FVoxelClimateGenerator::Sample(int32 InX, int32 InY) const
{
    const FVoxelGenerationSettings& Settings = Recipe->Settings;

    FVoxelClimateSample Result;

    Result.TemperatureQ15 = VoxelGeneration::Noise2D(
        Settings.Seed,
        InX,
        InY,
        Settings.ClimatePeriod,
        0x6D90B1191A5A1201ull);

    Result.MoistureQ15 = VoxelGeneration::Noise2D(
        Settings.Seed,
        InX,
        InY,
        Settings.ClimatePeriod,
        0x2169A938707B0147ull);

    Result.ContinentalnessQ15 = VoxelGeneration::Noise2D(
        Settings.Seed,
        InX,
        InY,
        Settings.ContinentalPeriod,
        0x9B8D72D6C5AE4C13ull);

    Result.ErosionQ15 = VoxelGeneration::Noise2D(
        Settings.Seed,
        InX,
        InY,
        Settings.ErosionPeriod,
        0x58C3EEB621C8109Dull);

    Result.RidgeQ15 = VoxelGeneration::Noise2D(
        Settings.Seed,
        InX,
        InY,
        Settings.MountainPeriod,
        0x7A835FDE28C04E51ull);

    return Result;
}
