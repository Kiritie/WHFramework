#include "Voxel/Rendering/VoxelProxyBuilder.h"

#include "Voxel/Generation/VoxelGenerationQuery.h"

namespace
{
	uint16 RecipeSymbol(
		const uint32 InValue)
	{
		return static_cast<uint16>(
			InValue &
			0xffffu);
	}
}

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
		OutError =
			TEXT("VoxelProxy Level 0 must use Fine Exact data");

		return false;
	}

	constexpr int32 GridSide = 16;

	FVoxelGenerationQuery Query;

	if (!FVoxelGenerationQuery::Create(
			Config,
			Cache,
			Query,
			OutError) ||
		!Query.Prepare(
			InKey.GetBounds(),
			OutError,
			InCancel))
	{
		return false;
	}

	FVoxelVoxelProxyData Data;

	Data.Key =
		InKey;

	Data.GridSide =
		GridSide;

	Data.Cells.SetNumUninitialized(
		GridSide *
		GridSide *
		GridSide);

	const int32 Step =
		InKey.GetStep();

	const FVoxelGenerationBounds Bounds =
		InKey.GetBounds();

	const FVoxelGenerationPalette& Palette =
		Config->Recipe->
			Palette;

	bool bHasProxyAir = false;
	bool bHasProxyNonAir = false;

	int32 OutputIndex = 0;

	for (int32 Z = 0;
		Z < GridSide;
		++Z)
	{
		if (InCancel &&
			InCancel->Load())
		{
			OutError =
				TEXT("Canceled");

			return false;
		}

		for (int32 Y = 0;
			Y < GridSide;
			++Y)
		{
			for (int32 X = 0;
				X < GridSide;
				++X)
			{
				TArray<uint32, TInlineAllocator<8>>
					Samples;

				const int32 Low =
					FMath::Max(
						0,
						Step /
							4);

				const int32 High =
					FMath::Clamp(
						Step -
							Low -
							1,
						0,
						Step - 1);

				const int32 Offsets[] =
				{
					Low,
					High
				};

				for (const int32 LocalZ :
					Offsets)
				{
					for (const int32 LocalY :
						Offsets)
					{
						for (const int32 LocalX :
							Offsets)
						{
							uint32 Value = 0;

							const FIntVector Position =
								Bounds.Min +
								FIntVector(
									X * Step +
										LocalX,
									Y * Step +
										LocalY,
									Z * Step +
										LocalZ);

							if (!Query.SampleSymbol(
								Position,
								Value,
								OutError))
							{
								return false;
							}

							Samples.Add(
								Value);
						}
					}
				}

				int32 AirCount = 0;
				int32 WaterCount = 0;
				int32 LavaCount = 0;
				int32 SolidCount = 0;

				uint32 FirstSolid = 0;

				for (const uint32 Sample :
					Samples)
				{
					const uint16 Symbol =
						RecipeSymbol(
							Sample);

					if (Symbol ==
						Palette.Air)
					{
						++AirCount;
					}
					else if (
						Symbol ==
						Palette.Water)
					{
						++WaterCount;
					}
					else if (
						Symbol ==
						Palette.Lava)
					{
						++LavaCount;
					}
					else
					{
						if (SolidCount == 0)
						{
							FirstSolid =
								Sample;
						}

						++SolidCount;
					}
				}

				uint32 Selected =
					Palette.Air;

				const int32 SampleCount =
					Samples.Num();

				if (SolidCount * 2 >=
					SampleCount)
				{
					Selected =
						FirstSolid;
				}
				else if (WaterCount ==
					SampleCount)
				{
					Selected =
						Palette.Water;
				}
				else if (LavaCount ==
					SampleCount)
				{
					Selected =
						Palette.Lava;
				}

				FVoxelBlockState& OutputState =
					Data.Cells[
						OutputIndex++];

				if (!Config->ToRuntime(
					Selected,
					OutputState))
				{
					OutError =
						TEXT("VoxelProxy generation returned an invalid recipe symbol");

					return false;
				}

				bHasProxyAir |=
					OutputState.IsAir();

				bHasProxyNonAir |=
					!OutputState.IsAir();
			}
		}
	}

	Data.bHasVisibleSurfaceEvidence =
		bHasProxyAir &&
		bHasProxyNonAir;

	OutData =
		MoveTemp(Data);

	OutError.Reset();
	return true;
}
