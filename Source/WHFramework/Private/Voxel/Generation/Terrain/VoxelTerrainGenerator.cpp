#include "Voxel/Generation/Terrain/VoxelTerrainGenerator.h"
#include "Voxel/Generation/Climate/VoxelClimateGenerator.h"
#include "Voxel/Generation/VoxelGenerationMath.h"

FVoxelTerrainGenerator::FVoxelTerrainGenerator(
    TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe,
    TSharedRef<const FVoxelClimateGenerator, ESPMode::ThreadSafe> InClimate)
    : Recipe(InRecipe)
    , Climate(InClimate)
{
}

FVoxelMacroTerrainSample FVoxelTerrainGenerator::SampleMacro(int32 InX, int32 InY) const
{
    const FVoxelClimateSample ClimateSample = Climate->Sample(InX, InY);

    FVoxelMacroTerrainSample Result;
    Result.SurfaceZ = SampleRawHeight(InX, InY, ClimateSample);
    Result.DensityHeight = Result.SurfaceZ;
    Result.SlopePermille = SampleSlopePermille(InX, InY);
    return Result;
}

int32 FVoxelTerrainGenerator::SampleDensityQ16(
    const FIntVector& InCell,
    const FVoxelMacroTerrainSample& InMacro) const
{
    return (InMacro.DensityHeight - InCell.Z) * 65536;
}

int32 FVoxelTerrainGenerator::SampleRawHeight(
    int32 InX,
    int32 InY,
    const FVoxelClimateSample& InClimate) const
{
    const FVoxelGenerationSettings& Settings = Recipe->Settings;

    const int32 Continental = InClimate.ContinentalnessQ15;
    const int32 Erosion = InClimate.ErosionQ15;
    const int32 Ridge = FMath::Abs(InClimate.RidgeQ15);

    const int32 ContinentalHeight = Continental * Settings.ContinentalAmplitude / 32768;

    const int32 MountainStrength = FMath::Clamp(
        (Ridge - 8192) * 2,
        0,
        32767);

    const int32 ErosionSuppression = FMath::Clamp(
        32767 - FMath::Abs(Erosion),
        4096,
        32767);

    const int32 MountainHeight =
        MountainStrength *
        ErosionSuppression /
        32767 *
        Settings.MountainAmplitude /
        32767;

    const int32 Detail = VoxelGeneration::Noise2D(
        Settings.Seed,
        InX,
        InY,
        Settings.DetailPeriod,
        0xC5D43E93B87F6291ull) *
        Settings.DetailAmplitude /
        32768;

    const int32 OceanBias = Continental < -12000
        ? (Continental + 12000) * Settings.ContinentalAmplitude / 32768
        : 0;

    return Settings.BaseHeight +
        ContinentalHeight +
        MountainHeight +
        Detail +
        OceanBias;
}

int32 FVoxelTerrainGenerator::SampleSlopePermille(int32 InX, int32 InY) const
{
    const FVoxelClimateSample CenterClimate = Climate->Sample(InX, InY);
    const int32 CenterHeight = SampleRawHeight(InX, InY, CenterClimate);

    const FVoxelClimateSample XClimate = Climate->Sample(InX + 4, InY);
    const FVoxelClimateSample YClimate = Climate->Sample(InX, InY + 4);

    const int32 XHeight = SampleRawHeight(InX + 4, InY, XClimate);
    const int32 YHeight = SampleRawHeight(InX, InY + 4, YClimate);

    const int32 Delta = FMath::Max(
        FMath::Abs(XHeight - CenterHeight),
        FMath::Abs(YHeight - CenterHeight));

    return FMath::Clamp(Delta * 250, 0, 1000);
}
