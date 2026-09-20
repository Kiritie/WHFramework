#include "Voxel/Rendering/VoxelProxyBuilder.h"

#include "Voxel/Generation/VoxelGenerationQuery.h"

FVoxelVoxelProxyBuilder::FVoxelVoxelProxyBuilder(
	TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
	TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> InCache)
	: Config(InConfig)
	, Cache(InCache)
{
}

bool FVoxelVoxelProxyBuilder::BuildNatural(
	const FVoxelViewKey& InKey,
	FVoxelVoxelProxyData& OutData,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	if (InKey.Level == 0)
	{
		OutError = TEXT("VoxelProxy Level 0 must use Fine Exact data");
		return false;
	}

	constexpr int32 GridSide = 16;
	FVoxelGenerationQuery Query;
	if (!FVoxelGenerationQuery::Create(Config, Cache, Query, OutError) ||
		!Query.Prepare(InKey.GetBounds(), OutError, InCancel))
	{
		return false;
	}

	FVoxelVoxelProxyData Data;
	Data.Key = InKey;
	Data.GridSide = GridSide;
	Data.Cells.SetNumUninitialized(GridSide * GridSide * GridSide);
	const int32 Step = InKey.GetStep();
	const FVoxelGenerationBounds Bounds = InKey.GetBounds();
	bool bHasAir = false;
	bool bHasSolid = false;
	int32 Index = 0;
	for (int32 Z = 0; Z < GridSide; ++Z)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		for (int32 Y = 0; Y < GridSide; ++Y)
		{
			for (int32 X = 0; X < GridSide; ++X)
			{
				const FIntVector Position = Bounds.Min + FIntVector(
					X * Step + Step / 2,
					Y * Step + Step / 2,
					Z * Step + Step / 2);
				uint32 RecipeValue = 0;
				if (!Query.SampleSymbol(Position, RecipeValue, OutError))
				{
					return false;
				}
				if (!Config->ToRuntime(RecipeValue, Data.Cells[Index++]))
				{
					OutError = TEXT("VoxelProxy generation returned an invalid recipe symbol");
					return false;
				}
				bHasAir |= Data.Cells[Index - 1].IsAir();
				bHasSolid |= !Data.Cells[Index - 1].IsAir();
			}
		}
	}
	Data.bHasVisibleSurfaceEvidence = bHasAir && bHasSolid;

	OutData = MoveTemp(Data);
	OutError.Reset();
	return true;
}
