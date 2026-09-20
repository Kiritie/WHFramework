#include "Voxel/Generation/Surface/VoxelSurfaceGenerator.h"

FVoxelSurfaceGenerator::FVoxelSurfaceGenerator(
    TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe)
    : Recipe(InRecipe)
{
}

void FVoxelSurfaceGenerator::ResolveColumn(FVoxelColumnSample& InOutColumn) const
{
    if (InOutColumn.SurfaceWaterZ != MIN_int32)
    {
        InOutColumn.bCoast =
            !InOutColumn.bRiver &&
            !InOutColumn.bLake &&
            FMath::Abs(
                InOutColumn.SurfaceZ -
                InOutColumn.SurfaceWaterZ) <= 2;
    }

    const FVoxelSurfaceRuntimeRuleSet* RuleSet =
        GetRuleSet(InOutColumn.BiomeIndex);

    if (!RuleSet)
    {
        InOutColumn.SurfaceMaterial =
            Recipe->Palette.Grass;
        return;
    }

    for (const FVoxelSurfaceRuntimeRule& Rule : RuleSet->Rules)
    {
        if (Matches(Rule, InOutColumn, 0))
        {
            InOutColumn.SurfaceMaterial = Rule.BlockSymbol;
            return;
        }
    }

    InOutColumn.SurfaceMaterial = Recipe->Palette.Grass;
}

uint32 FVoxelSurfaceGenerator::ResolveSymbol(
    const FIntVector& InCell,
    const FVoxelColumnSample& InColumn,
    int32 InDepthFromSurface) const
{
    const FVoxelSurfaceRuntimeRuleSet* RuleSet =
        GetRuleSet(InColumn.BiomeIndex);

    if (RuleSet)
    {
        for (const FVoxelSurfaceRuntimeRule& Rule : RuleSet->Rules)
        {
            if (!Matches(Rule, InColumn, InDepthFromSurface))
            {
                continue;
            }

			return Rule.BlockSymbol;
        }
    }

    if (InDepthFromSurface == 0)
    {
		return Recipe->Palette.Grass;
    }

    if (InDepthFromSurface <= 3)
    {
		return Recipe->Palette.Dirt;
    }

	return Recipe->Palette.Stone;
}

const FVoxelSurfaceRuntimeRuleSet* FVoxelSurfaceGenerator::GetRuleSet(uint16 InBiomeIndex) const
{
    if (!Recipe->Biomes.IsValidIndex(InBiomeIndex))
    {
        return nullptr;
    }

    const int32 RuleIndex =
        Recipe->Biomes[InBiomeIndex].SurfaceRuleIndex;

    return Recipe->SurfaceRules.IsValidIndex(RuleIndex)
        ? &Recipe->SurfaceRules[RuleIndex]
        : nullptr;
}

bool FVoxelSurfaceGenerator::Matches(
    const FVoxelSurfaceRuntimeRule& InRule,
    const FVoxelColumnSample& InColumn,
    int32 InDepthFromSurface) const
{
    if (!InRule.Height.Contains(InColumn.SurfaceZ) ||
        !InRule.Slope.Contains(InColumn.SlopePermille) ||
        !InRule.Temperature.Contains(InColumn.Climate.TemperatureQ15) ||
        !InRule.Moisture.Contains(InColumn.Climate.MoistureQ15) ||
        InDepthFromSurface < InRule.MinDepth ||
        InDepthFromSurface > InRule.MaxDepth)
    {
        return false;
    }

    if (InRule.bRiverOnly && !InColumn.bRiver)
    {
        return false;
    }

    if (InRule.bLakeOnly && !InColumn.bLake)
    {
        return false;
    }

    if (InRule.bOceanOnly && !InColumn.bOcean)
    {
        return false;
    }

    if (InRule.bCoastOnly && !InColumn.bCoast)
    {
        return false;
    }

    return true;
}
