#include "Voxel/Generation/Terrain/VoxelTerrainGenerator.h"
#include "Voxel/Generation/Climate/VoxelClimateGenerator.h"
#include "Voxel/Generation/Landform/VoxelLandform.h"
#include "Voxel/Generation/VoxelGenerationMath.h"

FVoxelTerrainGenerator::FVoxelTerrainGenerator(
    TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe,
    TSharedRef<const FVoxelClimateGenerator, ESPMode::ThreadSafe> InClimate)
    : Recipe(InRecipe)
    , Climate(InClimate)
    , Landform(InRecipe)
{
}

FVoxelLandformSample FVoxelTerrainGenerator::SampleLandform(int32 InX, int32 InY) const
{
    return Landform.Sample(InX, InY, Climate->Sample(InX, InY));
}

FVoxelMacroTerrainSample FVoxelTerrainGenerator::SampleMacro(int32 InX, int32 InY) const
{
    const FVoxelClimateSample ClimateSample = Climate->Sample(InX, InY);
    const FVoxelLandformSample LandformSample = Landform.Sample(InX, InY, ClimateSample);

    FVoxelMacroTerrainSample Result;
    Result.SurfaceZ = SampleRawHeight(InX, InY, ClimateSample, LandformSample);
    Result.DensityHeight = Result.SurfaceZ;
    Result.SlopePermille = SampleSlopePermille(InX, InY);
    Result.Landform = LandformSample;
    return Result;
}

int32 FVoxelTerrainGenerator::SampleDensityQ16(
    const FIntVector& InCell,
    const FVoxelMacroTerrainSample& InMacro) const
{
    const int64 Density = (int64(InMacro.DensityHeight) - InCell.Z + 1) * 65536;
    return int32(FMath::Clamp<int64>(Density, MIN_int32, MAX_int32));
}

int32 FVoxelTerrainGenerator::SampleRawHeight(
    int32 InX,
    int32 InY,
    const FVoxelClimateSample& InClimate,
    const FVoxelLandformSample& InLandform) const
{
    const FVoxelGenerationSettings& Settings = Recipe->Settings;
    const FVoxelLandformGenerationSettings& Shape = Settings.Landform;
    const int32 Relief = InLandform.ReliefQ15;
    const int32 Hills = VoxelGeneration::Noise2D(
        Settings.Seed, InX, InY, Shape.HillsPeriod, 0x41E3B521AC5D7039ull);
    const int32 HillDetail = VoxelGeneration::Noise2D(
        Settings.Seed, InX, InY, Shape.HillsDetailPeriod, 0xA6432C559FAC107Bull);
    const int32 ValleyNoise = VoxelGeneration::Noise2D(
        Settings.Seed, InX, InY, Shape.ValleyPeriod, 0x21DFAD741E6C03B5ull);
    const int32 Plateau = VoxelGeneration::Noise2D(
        Settings.Seed, InX, InY, Shape.PlateauPeriod, 0xA92162DC8805EE4Bull);
    const int32 Ridge = FMath::Abs(InClimate.RidgeQ15);
    const int32 Erosion = FMath::Clamp(
        32767 - FMath::Abs(InClimate.ErosionQ15), 8192, 32767);

    const int64 PlainHeight = static_cast<int64>(Relief) * Shape.PlainRelief / 32767;
    const int64 HillHeight = static_cast<int64>(Hills) * Shape.HillRelief / 32768 +
        static_cast<int64>(HillDetail) * Shape.HillsDetailRelief / 32768;
    const int64 HighlandHeight = Shape.HighlandUplift +
        static_cast<int64>(Hills) * Shape.HillRelief / 32768 +
        static_cast<int64>(Relief) * Shape.PlainRelief / 32768;
    const int64 RidgeSharp = static_cast<int64>(Ridge) * Ridge / 32767;
    const int64 MountainHeight = Shape.MountainBaseUplift +
        static_cast<int64>(Settings.MountainAmplitude) * RidgeSharp * Erosion /
            (32767ll * 32767) +
        static_cast<int64>(Relief) * Shape.HillRelief / 32768;
    const int64 PlateauHeight = Shape.PlateauUplift +
        static_cast<int64>(Plateau) * Shape.PlainRelief / 32767;
    const int64 BasinHeight = -static_cast<int64>(Shape.BasinDepth) +
        static_cast<int64>(Relief) * Shape.PlainRelief / 32767;

    int64 Height = static_cast<int64>(Settings.BaseHeight) +
        static_cast<int64>(InClimate.ContinentalnessQ15) * Settings.ContinentalAmplitude / 32768;
    Height += PlainHeight * InLandform.PlainQ15 / 32767;
    Height += HillHeight * InLandform.HillsQ15 / 32767;
    Height += HighlandHeight * InLandform.HighlandQ15 / 32767;
    Height += MountainHeight * InLandform.MountainQ15 / 32767;
    Height += PlateauHeight * InLandform.PlateauQ15 / 32767;
    Height += BasinHeight * InLandform.BasinQ15 / 32767;
    const int32 ValleyMask = FMath::Clamp(-ValleyNoise, 0, 32767);
    const int64 ValleyWeight = static_cast<int64>(InLandform.HillsQ15) +
        InLandform.HighlandQ15 + InLandform.MountainQ15;
    Height -= static_cast<int64>(ValleyMask) * Shape.ValleyDepth *
        FMath::Min<int64>(ValleyWeight, 32767) / (32767ll * 32767);

    Height += static_cast<int64>(VoxelGeneration::Noise2D(
        Settings.Seed,
        InX,
        InY,
        Settings.DetailPeriod,
        0xC5D43E93B87F6291ull)) * Settings.DetailAmplitude / 32768;

    const int32 Continental = InClimate.ContinentalnessQ15;
    const int64 OceanBias = Continental < -12000
        ? static_cast<int64>(Continental + 12000) * Settings.ContinentalAmplitude / 32768
        : 0;
    return static_cast<int32>(FMath::Clamp<int64>(
        Height + OceanBias, Settings.MinZ, static_cast<int64>(Settings.MaxZ) - 1));
}

int32 FVoxelTerrainGenerator::SampleSlopePermille(int32 InX, int32 InY) const
{
    const FVoxelClimateSample CenterClimate = Climate->Sample(InX, InY);
    const int32 CenterHeight = SampleRawHeight(
        InX, InY, CenterClimate, Landform.Sample(InX, InY, CenterClimate));

    // 边缘使用相同距离的单侧差分，不把域外样本 Clamp 成重复边缘 cell。
    const int32 SampleX = static_cast<int32>(VoxelGeneration::IsWorldCell(int64(InX) + 4) ? int64(InX) + 4 : int64(InX) - 4);
    const int32 SampleY = static_cast<int32>(VoxelGeneration::IsWorldCell(int64(InY) + 4) ? int64(InY) + 4 : int64(InY) - 4);
    const FVoxelClimateSample XClimate = Climate->Sample(SampleX, InY);
    const FVoxelClimateSample YClimate = Climate->Sample(InX, SampleY);

    const int32 XHeight = SampleRawHeight(
        SampleX, InY, XClimate, Landform.Sample(SampleX, InY, XClimate));
    const int32 YHeight = SampleRawHeight(
        InX, SampleY, YClimate, Landform.Sample(InX, SampleY, YClimate));

    const int64 Delta = FMath::Max(
        FMath::Abs(int64(XHeight) - CenterHeight),
        FMath::Abs(int64(YHeight) - CenterHeight));

    return int32(FMath::Clamp<int64>(Delta * 250, 0, 1000));
}
