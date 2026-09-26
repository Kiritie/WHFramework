#include "Voxel/Generation/Surface/VoxelSurfaceGenerator.h"
#include "Voxel/Generation/VoxelGenerationMath.h"

FVoxelSurfaceGenerator::FVoxelSurfaceGenerator(
    TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe)
    : Recipe(InRecipe)
{
}

void FVoxelSurfaceGenerator::ResolveColumn(const int32 InX, const int32 InY,
    FVoxelColumnSample& InOutColumn) const
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

    InOutColumn.SurfaceMaterial = static_cast<uint16>(
        ResolveSymbol(FIntVector(InX, InY, InOutColumn.SurfaceZ), InOutColumn, 0));
}

uint32 FVoxelSurfaceGenerator::ResolveSymbol(
    const FIntVector& InCell,
    const FVoxelColumnSample& InColumn,
    int32 InDepthFromSurface) const
{
    const bool bUnderWater = InColumn.SurfaceWaterZ != MIN_int32 &&
        InColumn.SurfaceWaterZ >= InColumn.SurfaceZ;
    const bool bKeepSubsoil = bUnderWater || InColumn.bOcean || InColumn.bLake ||
        InColumn.RiverZone == EVoxelRiverSurfaceZone::ChannelBed ||
        InColumn.RiverZone == EVoxelRiverSurfaceZone::WetMargin;
    const int32 SurfaceDepth = InDepthFromSurface == 0 && bKeepSubsoil
        ? 1 : InDepthFromSurface;
    const FVoxelSurfaceRuntimeRuleSet* RuleSet =
        GetRuleSet(InColumn.BiomeIndex);

    if (RuleSet)
    {
        for (const FVoxelSurfaceRuntimeRule& Rule : RuleSet->Rules)
        {
            if (Rule.RiverZone != EVoxelSurfaceRiverRule::Any &&
                Matches(Rule, InColumn, InDepthFromSurface))
            {
                return Rule.BlockSymbol;
            }
        }
    }

    if (InDepthFromSurface == 0 &&
        InColumn.RiverZone == EVoxelRiverSurfaceZone::DryBank &&
        Recipe->Palette.Sand != MAX_uint16 &&
        VoxelGeneration::RandomRange(VoxelGeneration::MakeSeed(
            Recipe->Settings.Seed,
            FIntVector(InCell.X, InCell.Y, InColumn.SurfaceZ),
            0x5249564552535552ull), 0, 99) < 35)
    {
        return Recipe->Palette.Sand;
    }

    if (RuleSet)
    {
        for (const FVoxelSurfaceRuntimeRule& Rule : RuleSet->Rules)
        {
            if (Rule.RiverZone != EVoxelSurfaceRiverRule::Any ||
                !Matches(Rule, InColumn, SurfaceDepth) ||
                (InDepthFromSurface == 0 && bKeepSubsoil &&
                    Rule.BlockSymbol == Recipe->Palette.Grass))
            {
                continue;
            }

			return Rule.BlockSymbol;
        }
    }

    if (SurfaceDepth == 0)
    {
		return Recipe->Biomes.IsValidIndex(InColumn.BiomeIndex)
			? Recipe->Biomes[InColumn.BiomeIndex].DefaultSurface
			: Recipe->Palette.Stone;
    }

    if (SurfaceDepth <= 3)
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

    if (InRule.RiverZone != EVoxelSurfaceRiverRule::Any &&
        static_cast<uint8>(InRule.RiverZone) !=
            static_cast<uint8>(InColumn.RiverZone) + 1)
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
