#include "Voxel/Generation/VoxelGenerationPipeline.h"

#include "Voxel/Generation/VoxelGenerationQuery.h"

FVoxelGenerationPipeline::FVoxelGenerationPipeline(
	TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
	TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> InCache)
	: Config(InConfig)
	, Cache(InCache)
{
}

bool FVoxelGenerationPipeline::GenerateSection(const FIntVector& InSectionCoordinate,
	TArray<FVoxelBlockState>& OutBaseBlocks, FString& OutError, const TAtomic<bool>* InCancel) const
{
	constexpr int32 SectionSide = 16;
	constexpr int32 SectionCellCount = SectionSide * SectionSide * SectionSide;
	const FIntVector WorldMin = InSectionCoordinate * SectionSide;
	const FVoxelGenerationBounds Bounds{WorldMin, WorldMin + FIntVector(SectionSide)};
	FVoxelGenerationQuery Query;
	if (!FVoxelGenerationQuery::Create(Config, Cache, Query, OutError) || !Query.Prepare(Bounds, OutError, InCancel))
	{
		return false;
	}

	TArray<FVoxelBlockState> Blocks;
	Blocks.SetNumUninitialized(SectionCellCount);
	int32 LocalIndex = 0;
	for (int32 Z = 0; Z < SectionSide; ++Z)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		for (int32 Y = 0; Y < SectionSide; ++Y)
		{
			for (int32 X = 0; X < SectionSide; ++X)
			{
				uint32 RecipeValue = 0;
				if (!Query.SampleSymbol(WorldMin + FIntVector(X, Y, Z), RecipeValue, OutError))
				{
					return false;
				}
				if (!Config->ToRuntime(RecipeValue, Blocks[LocalIndex++]))
				{
					OutError = TEXT("Voxel generation returned an invalid recipe symbol");
					return false;
				}
			}
		}
	}
	OutBaseBlocks = MoveTemp(Blocks);
	OutError.Reset();
	return true;
}

bool FVoxelGenerationPipeline::SampleColumn(int32 InX, int32 InY, FVoxelColumnSample& OutColumn,
	FString& OutError, const TAtomic<bool>* InCancel) const
{
	const FVoxelGenerationBounds Bounds{FIntVector(InX, InY, Config->Recipe->Settings.MinZ),
		FIntVector(InX + 1, InY + 1, Config->Recipe->Settings.MaxZ)};
	FVoxelGenerationQuery Query;
	if (!FVoxelGenerationQuery::Create(Config, Cache, Query, OutError) || !Query.Prepare(Bounds, OutError, InCancel))
	{
		return false;
	}
	return Query.SampleColumn(InX, InY, OutColumn, OutError);
}

bool FVoxelGenerationPipeline::SampleBlock(
	const FIntVector& InPosition,
	FVoxelBlockState& OutState,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	FVoxelGenerationQuery Query;
	const FVoxelGenerationBounds Bounds { InPosition, InPosition + FIntVector(1) };
	if (!FVoxelGenerationQuery::Create(Config, Cache, Query, OutError) ||
		!Query.Prepare(Bounds, OutError, InCancel))
	{
		return false;
	}

	uint32 RecipeValue = 0;
	if (!Query.SampleSymbol(InPosition, RecipeValue, OutError) ||
		!Config->ToRuntime(RecipeValue, OutState))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Voxel generation returned an invalid recipe symbol");
		}
		return false;
	}

	OutError.Reset();
	return true;
}
