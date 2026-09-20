#pragma once

#include "Voxel/Generation/VoxelGenerationPipeline.h"
#include "Voxel/Runtime/VoxelRegistry.h"

namespace VoxelTest
{
	inline TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> MakeGenerationConfig(
		const int32 InSeed = 173)
	{
		FVoxelGenerationRecipe Recipe;
		Recipe.RecipeHash = 0x8a42d3c871f0195bull;
		Recipe.Settings.Seed = InSeed;
		Recipe.Settings.MinZ = -64;
		Recipe.Settings.MaxZ = 128;
		Recipe.Settings.SeaLevel = 0;
		Recipe.Settings.BaseHeight = 32;
		Recipe.Settings.HydrologyCellSize = 8;
		Recipe.Settings.HydrologyRegionSide = 8;
		Recipe.Settings.HydrologyHaloCells = 8;
		Recipe.Settings.HydrologySinkSpacing = 8;
		Recipe.Settings.RiverSourceAccumulation = 8;
		Recipe.Settings.RiverSourceSpacing = 4;
		Recipe.Settings.RiverTraceBudget = 128;
		Recipe.Settings.LakeMaxCells = 256;
		Recipe.Settings.CaveSpacing = 32;
		Recipe.Settings.CaveMinDepth = 8;
		Recipe.Settings.CaveMaxDepth = 32;
		Recipe.Settings.CaveMainRadius = 4;
		Recipe.Settings.CaveBranchRadius = 3;
		Recipe.Settings.AquiferSpacing = 32;
		Recipe.Settings.AquiferRadius = 8;
		Recipe.Settings.LavaCeiling = -48;
		Recipe.Settings.Ecology.Tree.bEnabled = false;
		Recipe.Settings.Ecology.Grass.bEnabled = false;
		Recipe.BlockNames =
		{
			NAME_None,
			TEXT("test:stone"),
			TEXT("test:dirt"),
			TEXT("test:grass"),
			TEXT("test:sand"),
			TEXT("test:snow"),
			TEXT("test:water"),
			TEXT("test:lava"),
			TEXT("test:bedrock"),
			TEXT("test:road")
		};
		Recipe.Palette.Stone = 1;
		Recipe.Palette.Dirt = 2;
		Recipe.Palette.Grass = 3;
		Recipe.Palette.Sand = 4;
		Recipe.Palette.Snow = 5;
		Recipe.Palette.Water = 6;
		Recipe.Palette.Lava = 7;
		Recipe.Palette.Bedrock = 8;
		Recipe.Palette.Road = 9;
		FVoxelBiomeRuntimeDefinition Biome;
		Biome.StableId = TEXT("test:biome");
		Recipe.Biomes.Add(Biome);
		FString Error;
		check(Recipe.BuildLookups(Error));

		FVoxelGenerationRuntimeConfig Config;
		Config.Recipe = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe));
		Config.RegistryHash = 0x712de49a9c48b365ull;
		Config.SymbolToRuntime.SetNum(10);
		Config.RuntimeToSymbol.SetNum(10);
		for (uint16 Index = 0; Index < 10; ++Index)
		{
			Config.SymbolToRuntime[Index] = Index;
			Config.RuntimeToSymbol[Index] = Index;
		}
		Config.Air = FVoxelBlockState();
		Config.Stone = FVoxelBlockState(1, 0);
		Config.Dirt = FVoxelBlockState(2, 0);
		Config.Grass = FVoxelBlockState(3, 0);
		Config.Sand = FVoxelBlockState(4, 0);
		Config.Snow = FVoxelBlockState(5, 0);
		Config.Water = FVoxelBlockState(6, 0);
		Config.Lava = FVoxelBlockState(7, 0);
		Config.Bedrock = FVoxelBlockState(8, 0);
		Config.Road = FVoxelBlockState(9, 0);
		return MakeShared<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe>(MoveTemp(Config));
	}

	inline TSharedRef<FVoxelGenerationPipeline, ESPMode::ThreadSafe> MakeGenerator(const int32 InSeed = 173)
	{
		return MakeShared<FVoxelGenerationPipeline, ESPMode::ThreadSafe>(
			MakeGenerationConfig(InSeed),
			MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>());
	}

	inline TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> MakeRegistry()
	{
		FVoxelRegistrySnapshot Registry;
		Registry.Hash = 0x712de49a9c48b365ull;
		Registry.Definitions.SetNum(10);
		for (uint16 Index = 0; Index < 10; ++Index)
		{
			Registry.Definitions[Index].TypeId = Index;
			Registry.Definitions[Index].BlockName = Index == 0 ? FName(TEXT("core:air")) :
				FName(*FString::Printf(TEXT("test:block_%u"), Index));
			Registry.Names.Add(Registry.Definitions[Index].BlockName, Index);
		}
		return MakeShared<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe>(MoveTemp(Registry));
	}

	inline uint64 HashBlocks(TConstArrayView<FVoxelBlockState> InBlocks)
	{
		uint64 Hash = 1469598103934665603ull;
		for (const FVoxelBlockState& Block : InBlocks)
		{
			Hash ^= Block.Pack();
			Hash *= 1099511628211ull;
		}
		return Hash;
	}
}
