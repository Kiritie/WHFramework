#include "Voxel/Rendering/VoxelProxyBuilder.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "Voxel/Generation/VoxelGenerationQuery.h"

uint64 FVoxelVoxelProxyData::GetAllocatedBytes() const
{
	uint64 Bytes =
		Cells.GetAllocatedSize();

	for (const TArray<FVoxelBlockState>& Face :
		Halo)
	{
		Bytes +=
			Face.GetAllocatedSize();
	}

	return Bytes;
}

FVoxelVoxelProxyBuilder::FVoxelVoxelProxyBuilder(
	TSharedRef<
		const FVoxelGenerationRuntimeConfig,
		ESPMode::ThreadSafe> InConfig,
	TSharedRef<
		FVoxelGenerationPlanCache,
		ESPMode::ThreadSafe> InCache)
	: Config(InConfig)
	, Cache(InCache)
{
}

bool FVoxelVoxelProxyBuilder::Build(const FVoxelViewKey& InKey, const FVoxelOverlaySnapshotSet& InOverlays,
	FVoxelVoxelProxyData& OutData, FString& OutError, const TAtomic<bool>* InCancel) const
{
	FVoxelVoxelProxyData Data;
	if (!BuildNatural(InKey, Data, OutError, InCancel)) return false;
	const FIntVector Origin = InKey.GetBounds().Min;
	const int32 Step = InKey.GetStep();
	for (int32 Z = 0; Z < 16; ++Z)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		for (int32 Y = 0; Y < 16; ++Y)
		{
			for (int32 X = 0; X < 16; ++X)
			{
				InOverlays.ApplyAt(Origin + FIntVector(X, Y, Z) * Step + FIntVector(Step / 2),
					Data.Cells[X + Y * 16 + Z * 256]);
			}
		}
	}
	for (int32 Face = 0; Face < 6; ++Face)
	{
		if (!Data.Known[Face]) continue;
		const int32 Axis = Face / 2;
		for (int32 Y = 0; Y < 16; ++Y)
		{
			for (int32 X = 0; X < 16; ++X)
			{
				FIntVector Local = FIntVector::ZeroValue;
				Local[Axis] = Face % 2 == 0 ? 16 : -1;
				Local[(Axis + 1) % 3] = X;
				Local[(Axis + 2) % 3] = Y;
				InOverlays.ApplyAt(Origin + Local * Step + FIntVector(Step / 2), Data.Halo[Face][X + Y * 16]);
			}
		}
	}
	bool bHasAir = false;
	bool bHasSolid = false;
	for (const FVoxelBlockState State : Data.Cells)
	{
		bHasAir |= State.IsAir();
		bHasSolid |= !State.IsAir() && State != Config->Water && State != Config->Lava;
	}
	Data.bHasVisibleSurfaceEvidence = bHasAir && bHasSolid;
	OutData = MoveTemp(Data);
	return true;
}

bool FVoxelVoxelProxyBuilder::BuildNatural(
	const FVoxelViewKey& InKey,
	FVoxelVoxelProxyData& OutData,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(
		Voxel_ProxyBuild);

	if (InKey.Level == 0)
	{
		OutError =
			TEXT(
				"VoxelProxy Level 0 must use Fine Exact data");

		return false;
	}

	constexpr int32 GridSide =
		16;

	constexpr int32 ColumnSampleSide =
		GridSide + 2;

	const int32 Step =
		InKey.GetStep();

	const FVoxelGenerationBounds Bounds =
		InKey.GetBounds();

	FVoxelGenerationQuery Query;

	if (!FVoxelGenerationQuery::Create(
			Config,
			Cache,
			Query,
			OutError))
	{
		return false;
	}

	/**
	 * 多采一圈 XY Column。
	 * Z halo 仍然复用中心 16x16 columns。
	 */
	const FVoxelGenerationBounds ColumnBounds {
		FIntVector(
			Bounds.Min.X -
				Step,
			Bounds.Min.Y -
				Step,
			Config->Recipe->
				Settings.MinZ),
		FIntVector(
			Bounds.Max.X +
				Step,
			Bounds.Max.Y +
				Step,
			Config->Recipe->
				Settings.MaxZ)
	};

	if (!Query.PrepareColumns(
			ColumnBounds,
			OutError,
			InCancel))
	{
		return false;
	}

	TArray<FVoxelColumnSample> Columns;

	Columns.SetNumUninitialized(
		ColumnSampleSide *
		ColumnSampleSide);

	auto ColumnIndex =
		[](
			const int32 InLocalX,
			const int32 InLocalY)
		{
			return
				(InLocalX + 1) +
				(InLocalY + 1) *
					ColumnSampleSide;
		};

	for (int32 LocalY = -1;
		LocalY <= GridSide;
		++LocalY)
	{
		if (InCancel &&
			InCancel->Load())
		{
			OutError =
				TEXT("Canceled");

			return false;
		}

		for (int32 LocalX = -1;
			LocalX <= GridSide;
			++LocalX)
		{
			const int32 WorldX =
				Bounds.Min.X +
				LocalX *
					Step +
				Step /
					2;

			const int32 WorldY =
				Bounds.Min.Y +
				LocalY *
					Step +
				Step /
					2;

			if (!Query.SampleColumn(
					WorldX,
					WorldY,
					Columns[
						ColumnIndex(
							LocalX,
							LocalY)],
					OutError))
			{
				return false;
			}
		}
	}

	auto ResolveCell =
		[
			this,
			&Bounds,
			Step,
			&Columns,
			&ColumnIndex
		](
			const int32 InLocalX,
			const int32 InLocalY,
			const int32 InLocalZ,
			FVoxelBlockState& OutState,
			FString& OutCellError)
		{
			if (InLocalX < -1 ||
				InLocalX > GridSide ||
				InLocalY < -1 ||
				InLocalY > GridSide ||
				InLocalZ < -1 ||
				InLocalZ > GridSide)
			{
				OutCellError =
					TEXT(
						"Voxel proxy halo local sample is outside one-cell border");

				return false;
			}

			const FVoxelColumnSample& Column =
				Columns[
					ColumnIndex(
						InLocalX,
						InLocalY)];

			const int32 CellMinZ =
				Bounds.Min.Z +
				InLocalZ *
					Step;

			const int32 CellMaxZ =
				CellMinZ +
				Step -
				1;

			FVoxelBlockState State =
				Config->Air;

			if (CellMinZ <=
				Column.SurfaceZ)
			{
				if (CellMaxZ >=
					Column.SurfaceZ)
				{
					if (!Config->
						ToRuntime(
							Column.
								SurfaceMaterial,
							State))
					{
						OutCellError =
							TEXT(
								"VoxelProxy surface symbol is invalid");

						return false;
					}
				}
				else
				{
					State =
						Config->
							Stone;
				}
			}
			else if (
				Column.SurfaceWaterZ !=
					MIN_int32 &&
				CellMinZ <=
					Column.
						SurfaceWaterZ)
			{
				State =
					Config->
						Water;
			}

			OutState =
				State;

			return true;
		};

	FVoxelVoxelProxyData Data;

	Data.Key =
		InKey;

	Data.GridSide =
		GridSide;

	Data.Cells.SetNumUninitialized(
		GridSide *
		GridSide *
		GridSide);

	bool bHasAir =
		false;

	bool bHasSolid =
		false;

	for (int32 Z = 0;
		Z < GridSide;
		++Z)
	{
		for (int32 Y = 0;
			Y < GridSide;
			++Y)
		{
			for (int32 X = 0;
				X < GridSide;
				++X)
			{
				FVoxelBlockState State;

				if (!ResolveCell(
						X,
						Y,
						Z,
						State,
						OutError))
				{
					return false;
				}

				const int32 Index =
					X +
					Y *
						GridSide +
					Z *
						GridSide *
						GridSide;

				Data.Cells[
					Index] =
						State;

				bHasAir |=
					State.IsAir();

				bHasSolid |=
					!State.IsAir() &&
					State !=
						Config->Water &&
					State !=
						Config->Lava;
			}
		}
	}

	/**
	 * 六个 coarse halo face。
	 * index layout 与 FVoxelSectionSnapshot::TrySample 完全一致。
	 */
	for (int32 Face = 0;
		Face < 6;
		++Face)
	{
		const int32 Axis =
			Face /
			2;

		const bool bNegative =
			(Face &
				1) !=
			0;

		const int32 U =
			(Axis + 1) %
			3;

		const int32 V =
			(Axis + 2) %
			3;

		Data.Halo[
			Face].
			SetNumUninitialized(
				GridSide *
				GridSide);

		for (int32 LocalV = 0;
			LocalV < GridSide;
			++LocalV)
		{
			for (int32 LocalU = 0;
				LocalU < GridSide;
				++LocalU)
			{
				FIntVector Local(
					0,
					0,
					0);

				Local[Axis] =
					bNegative
						? -1
						: GridSide;

				Local[U] =
					LocalU;

				Local[V] =
					LocalV;

				FVoxelBlockState State;

				if (!ResolveCell(
						Local.X,
						Local.Y,
						Local.Z,
						State,
						OutError))
				{
					return false;
				}

				Data.Halo[
					Face][
						LocalU +
						LocalV *
							GridSide] =
								State;
			}
		}

		Data.Known[
			Face] =
				true;
	}

	Data.bHasVisibleSurfaceEvidence =
		bHasAir &&
		bHasSolid;

	OutData =
		MoveTemp(
			Data);

	OutError.Reset();
	return true;
}
