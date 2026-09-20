#include "Voxel/Generation/Biome/VoxelBiomeGenerator.h"

FVoxelBiomeGenerator::FVoxelBiomeGenerator(TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe)
    : Recipe(InRecipe)
{
}

uint16 FVoxelBiomeGenerator::Resolve(
    const FVoxelClimateSample& InClimate,
    const FVoxelMacroTerrainSample& InTerrain) const
{
    int32 BestPriority = MIN_int32;
    uint16 BestIndex = MAX_uint16;

    for (int32 Index = 0; Index < Recipe->Biomes.Num(); ++Index)
    {
        const FVoxelBiomeRuntimeDefinition& Biome = Recipe->Biomes[Index];

        if (!Matches(Biome, InClimate, InTerrain))
        {
            continue;
        }

        if (Biome.Priority > BestPriority)
        {
            BestPriority = Biome.Priority;
            BestIndex = static_cast<uint16>(Index);
        }
    }

    return BestIndex;
}

FName FVoxelBiomeGenerator::GetBiomeId(uint16 InBiomeIndex) const
{
    if (!Recipe->Biomes.IsValidIndex(InBiomeIndex))
    {
        return NAME_None;
    }

    return Recipe->Biomes[InBiomeIndex].StableId;
}

bool FVoxelBiomeGenerator::Matches(
    const FVoxelBiomeRuntimeDefinition& InBiome,
    const FVoxelClimateSample& InClimate,
    const FVoxelMacroTerrainSample& InTerrain) const
{
    return InBiome.Temperature.Contains(InClimate.TemperatureQ15) &&
        InBiome.Moisture.Contains(InClimate.MoistureQ15) &&
        InBiome.Continentalness.Contains(InClimate.ContinentalnessQ15) &&
        InBiome.Erosion.Contains(InClimate.ErosionQ15) &&
        InBiome.Height.Contains(InTerrain.SurfaceZ) &&
        InBiome.Slope.Contains(InTerrain.SlopePermille);
}
