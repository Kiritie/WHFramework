#include "Voxel/Rendering/VoxelProxyBuilder.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
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
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ProxyBuild);

	if (InKey.Level == 0)
	{
		OutError = TEXT("VoxelProxy Level 0 must use Fine Exact data");
		return false;
	}

	constexpr int32 GridSide = 16;
	const int32 Step = InKey.GetStep();
	const FVoxelGenerationBounds Bounds = InKey.GetBounds();
	FVoxelGenerationQuery Query;
	if (!FVoxelGenerationQuery::Create(Config, Cache, Query, OutError))
	{
		return false;
	}

	const FVoxelGenerationBounds ColumnBounds {
		FIntVector(Bounds.Min.X, Bounds.Min.Y, Config->Recipe->Settings.MinZ),
		FIntVector(Bounds.Max.X, Bounds.Max.Y, Config->Recipe->Settings.MaxZ)
	};
	if (!Query.PrepareColumns(ColumnBounds, OutError, InCancel))
	{
		return false;
	}

	FVoxelVoxelProxyData Data;
	Data.Key = InKey;
	Data.GridSide = GridSide;
	Data.Cells.SetNumUninitialized(GridSide * GridSide * GridSide);

	TArray<FVoxelColumnSample> Columns;
	Columns.SetNumUninitialized(GridSide * GridSide);
	for (int32 Y = 0; Y < GridSide; ++Y)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}

		for (int32 X = 0; X < GridSide; ++X)
		{
			const int32 WorldX = Bounds.Min.X + X * Step + Step / 2;
			const int32 WorldY = Bounds.Min.Y + Y * Step + Step / 2;
			if (!Query.SampleColumn(WorldX, WorldY, Columns[X + Y * GridSide], OutError))
			{
				return false;
			}
		}
	}

	bool bHasAir = false;
	bool bHasSolid = false;
	for (int32 Z = 0; Z < GridSide; ++Z)
	{
		for (int32 Y = 0; Y < GridSide; ++Y)
		{
			for (int32 X = 0; X < GridSide; ++X)
			{
				const FVoxelColumnSample& Column = Columns[X + Y * GridSide];
				const int32 CellMinZ = Bounds.Min.Z + Z * Step;
				const int32 CellMaxZ = CellMinZ + Step - 1;
				FVoxelBlockState State = Config->Air;
				if (CellMinZ <= Column.SurfaceZ)
				{
					if (CellMaxZ >= Column.SurfaceZ)
					{
						if (!Config->ToRuntime(Column.SurfaceMaterial, State))
						{
							OutError = TEXT("VoxelProxy surface symbol is invalid");
							return false;
						}
					}
					else
					{
						State = Config->Stone;
					}
				}
				else if (Column.SurfaceWaterZ != MIN_int32 && CellMinZ <= Column.SurfaceWaterZ)
				{
					State = Config->Water;
				}

				const int32 Index = X + Y * GridSide + Z * GridSide * GridSide;
				Data.Cells[Index] = State;
				bHasAir |= State.IsAir();
				bHasSolid |= !State.IsAir() && State != Config->Water && State != Config->Lava;
			}
		}
	}

	Data.bHasVisibleSurfaceEvidence = bHasAir && bHasSolid;
	OutData = MoveTemp(Data);
	OutError.Reset();
	return true;
}
