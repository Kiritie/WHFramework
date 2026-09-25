#include "Voxel/Rendering/VoxelProxyBuilder.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"
#include "Voxel/Generation/VoxelGenerationPipeline.h"
#include "Voxel/Generation/Ecology/VoxelEcology.h"

namespace
{
	struct FCoarseOverlayCell
	{
		int32 Removed = 0;
		TMap<uint32, int32> Placed;

		void Add(const FVoxelBlockState& InState)
		{
			if (InState.IsAir())
			{
				++Removed;
			}
			else
			{
				++Placed.FindOrAdd(InState.Pack());
			}
		}

		void Apply(const int32 InStep, const bool bArchitecture,
			const FVoxelBlockState InTrunk, const FVoxelBlockState InLeaves,
			FVoxelBlockState& InOutState) const
		{
			int32 BestCount = 0;
			uint32 BestState = 0;
			for (const TPair<uint32, int32>& Pair : Placed)
			{
				if (Pair.Value > BestCount ||
					(Pair.Value == BestCount && Pair.Key < BestState))
				{
					BestCount = Pair.Value;
					BestState = Pair.Key;
				}
			}
			const int32 RemovalThreshold = InOutState == InTrunk || InOutState == InLeaves ? 1 :
				(bArchitecture ? FMath::Max(1, InStep / 2) :
					FMath::Max(1, (InStep * InStep + 1) / 2));
			if (!InOutState.IsAir() && Removed >= RemovalThreshold &&
				Removed >= BestCount)
			{
				InOutState = FVoxelBlockState();
			}
			else if (BestCount >= FMath::Max(1, InStep / 4) &&
				(InOutState.IsAir() || BestCount > Removed))
			{
				InOutState = FVoxelBlockState::Unpack(BestState);
			}
		}
	};

	void ApplyCoarseEdits(
		const TMap<FIntVector, FCoarseOverlayCell>& InEdits,
		const TMap<FIntVector, FVoxelBlockState>& InArchitecture,
		const int32 InStep,
		const FVoxelBlockState InTrunk,
		const FVoxelBlockState InLeaves,
		FVoxelVoxelProxyData& InOutData)
	{
		for (int32 Z = 0; Z < 16; ++Z)
		{
			for (int32 Y = 0; Y < 16; ++Y)
			{
				for (int32 X = 0; X < 16; ++X)
				{
					if (const FCoarseOverlayCell* Edits = InEdits.Find(FIntVector(X, Y, Z)))
					{
						const FIntVector Local(X, Y, Z);
						Edits->Apply(InStep, InArchitecture.Contains(Local), InTrunk,
							InLeaves, InOutData.Cells[X + Y * 16 + Z * 256]);
					}
				}
			}
		}
		for (int32 Face = 0; Face < 6; ++Face)
		{
			if (!InOutData.Known[Face]) continue;
			const int32 Axis = Face / 2;
			for (int32 Y = 0; Y < 16; ++Y)
			{
				for (int32 X = 0; X < 16; ++X)
				{
					FIntVector Local = FIntVector::ZeroValue;
					Local[Axis] = Face % 2 == 0 ? 16 : -1;
					Local[(Axis + 1) % 3] = X;
					Local[(Axis + 2) % 3] = Y;
					if (const FCoarseOverlayCell* Edits = InEdits.Find(Local))
					{
							Edits->Apply(InStep, InArchitecture.Contains(Local), InTrunk,
								InLeaves, InOutData.Halo[Face][X + Y * 16]);
					}
				}
			}
		}
	}
}

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
		ESPMode::ThreadSafe> InCache,
	TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator)
	: Config(InConfig)
	, Cache(InCache)
	, Generator(MoveTemp(InGenerator))
{
}

bool FVoxelVoxelProxyBuilder::Build(const FVoxelViewKey& InKey, const FVoxelOverlaySnapshotSet& InOverlays,
	FVoxelVoxelProxyData& OutData, FString& OutError, const TAtomic<bool>* InCancel) const
{
	FVoxelVoxelProxyData Data;
	if (!BuildNatural(InKey, Data, OutError, InCancel)) return false;
	if (!ApplyTreeSilhouettes(InKey, InOverlays, Data, OutError, InCancel)) return false;
	const FIntVector Origin = InKey.GetBounds().Min;
	const int32 Step = InKey.GetStep();
	TMap<FIntVector, FVoxelBlockState> CoarseNatural;
	if (Generator && !Generator->BuildCoarseOverlay(
			Origin, Step, 16, CoarseNatural, OutError, InCancel))
	{
		return false;
	}
	for (const TPair<FIntVector, FVoxelBlockState>& Entry : CoarseNatural)
	{
		const FIntVector& Local = Entry.Key;
		if (Local.X >= 0 && Local.X < 16 && Local.Y >= 0 && Local.Y < 16 &&
			Local.Z >= 0 && Local.Z < 16)
		{
			Data.Cells[Local.X + Local.Y * 16 + Local.Z * 256] = Entry.Value;
		}
		for (int32 Face = 0; Face < 6; ++Face)
		{
			const int32 Axis = Face / 2;
			const int32 U = (Axis + 1) % 3;
			const int32 V = (Axis + 2) % 3;
			if (Data.Known[Face] && Local[Axis] == (Face % 2 == 0 ? 16 : -1) &&
				Local[U] >= 0 && Local[U] < 16 && Local[V] >= 0 && Local[V] < 16)
			{
				Data.Halo[Face][Local[U] + Local[V] * 16] = Entry.Value;
			}
		}
	}
	TMap<FIntVector, FCoarseOverlayCell> CoarseEdits;
	for (const TPair<FIntVector, FVoxelOverlaySnapshot>& Section : InOverlays.Sections)
	{
		const FIntVector SectionOrigin = Section.Key * 16;
		for (const TPair<int32, FVoxelBlockState>& Edit : Section.Value.Blocks)
		{
			const int32 Index = Edit.Key;
			const FIntVector Position = SectionOrigin + FIntVector(
				Index % 16, (Index / 16) % 16, Index / 256);
			const FIntVector Coarse(
				VoxelGeneration::FloorDivide(Position.X - Origin.X, Step),
				VoxelGeneration::FloorDivide(Position.Y - Origin.Y, Step),
				VoxelGeneration::FloorDivide(Position.Z - Origin.Z, Step));
			if (Coarse.X < -1 || Coarse.X > 16 || Coarse.Y < -1 || Coarse.Y > 16 ||
				Coarse.Z < -1 || Coarse.Z > 16)
			{
				continue;
			}
			CoarseEdits.FindOrAdd(Coarse).Add(Edit.Value);
		}
	}
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	FVoxelBlockState Trunk;
	FVoxelBlockState Leaves;
	Config->ToRuntime(Config->Recipe->Ecology.TreeTrunk, Trunk);
	Config->ToRuntime(Config->Recipe->Ecology.TreeLeaves, Leaves);
	ApplyCoarseEdits(CoarseEdits, CoarseNatural, Step, Trunk, Leaves, Data);
	bool bHasAir = false;
	bool bHasSolid = false;
	for (const FVoxelBlockState State : Data.Cells)
	{
		bHasAir |= State.IsAir() || State == Config->Water || State == Config->Lava;
		bHasSolid |= !State.IsAir() && State != Config->Water && State != Config->Lava;
	}
	Data.bHasVisibleSurfaceEvidence = bHasAir && bHasSolid;
	OutData = MoveTemp(Data);
	return true;
}

bool FVoxelVoxelProxyBuilder::ApplyTreeSilhouettes(
	const FVoxelViewKey& InKey,
	const FVoxelOverlaySnapshotSet& InOverlays,
	FVoxelVoxelProxyData& InOutData,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	const FVoxelTreeGenerationSettings& Trees = Config->Recipe->Settings.Ecology.Tree;
	if (!Trees.bEnabled || Trees.ChancePermille <= 0 || Trees.DensityPermille <= 0)
	{
		return true;
	}
	FVoxelBlockState Trunk;
	FVoxelBlockState Leaves;
	if (!Config->ToRuntime(Config->Recipe->Ecology.TreeTrunk, Trunk) ||
		!Config->ToRuntime(Config->Recipe->Ecology.TreeLeaves, Leaves))
	{
		OutError = TEXT("Tree silhouette symbols are invalid");
		return false;
	}
	FVoxelGenerationQuery Query;
	if (!FVoxelGenerationQuery::Create(Config, Cache, Query, OutError))
	{
		return false;
	}
	const FVoxelGenerationBounds Bounds = InKey.GetBounds();
	const int32 Step = InKey.GetStep();
	const FIntVector Origin = Bounds.Min;
	const FVoxelGenerationBounds TreeBounds {
		FIntVector(Bounds.Min.X - Trees.CrownRadius,
			Bounds.Min.Y - Trees.CrownRadius, Config->Recipe->Settings.MinZ),
		FIntVector(Bounds.Max.X + Trees.CrownRadius,
			Bounds.Max.Y + Trees.CrownRadius, Config->Recipe->Settings.MaxZ)
	};
	bool bSampleFailed = false;
	auto SampleColumn = [&Query, &OutError, &bSampleFailed, InCancel](
		const FIntVector& Position, FVoxelColumnSample& Column)
	{
		if (!Query.SampleEnvironmentColumn(Position.X, Position.Y, Column,
			OutError, InCancel))
		{
			bSampleFailed = true;
			return false;
		}
		return true;
	};
	auto SampleBase = [&SampleColumn, this](const FIntVector& Position,
		uint32& OutSymbol)
	{
		FVoxelColumnSample Column;
		if (!SampleColumn(Position, Column)) return false;
		OutSymbol = Position.Z <= Column.SurfaceZ ?
			Config->Recipe->Palette.Stone : Config->Recipe->Palette.Air;
		return true;
	};
	auto Draw = [&InOutData, &Origin, Step, Trunk, Leaves,
		Water = Config->Water](
		const FIntVector& Position, const FVoxelBlockState State,
		const bool bReplaceTrunk)
	{
		const FIntVector Local(
			VoxelGeneration::FloorDivide(Position.X - Origin.X, Step),
			VoxelGeneration::FloorDivide(Position.Y - Origin.Y, Step),
			VoxelGeneration::FloorDivide(Position.Z - Origin.Z, Step));
		if (Local.X >= 0 && Local.X < 16 && Local.Y >= 0 && Local.Y < 16 &&
			Local.Z >= 0 && Local.Z < 16)
		{
			FVoxelBlockState& Cell = InOutData.Cells[Local.X + Local.Y * 16 + Local.Z * 256];
			if (Cell.IsAir() || Cell == Water || (State == Trunk && Cell == Leaves) ||
				(bReplaceTrunk && Cell == Trunk)) Cell = State;
		}
		for (int32 Face = 0; Face < 6; ++Face)
		{
			const int32 Axis = Face / 2;
			const int32 U = (Axis + 1) % 3;
			const int32 V = (Axis + 2) % 3;
			if (InOutData.Known[Face] && Local[Axis] == (Face % 2 == 0 ? 16 : -1) &&
				Local[U] >= 0 && Local[U] < 16 && Local[V] >= 0 && Local[V] < 16)
			{
				FVoxelBlockState& Cell = InOutData.Halo[Face][Local[U] + Local[V] * 16];
				if (Cell.IsAir() || Cell == Water || (State == Trunk && Cell == Leaves) ||
					(bReplaceTrunk && Cell == Trunk)) Cell = State;
			}
		}
	};
	const FVoxelEcologyGenerator Ecology(Config->Recipe.ToSharedRef());
	int32 Candidates = 0;
	int32 Accepted = 0;
	Ecology.EnumerateTrees(TreeBounds, SampleColumn, SampleBase,
		[&](const FIntVector& Anchor, const int32 Height, const FVoxelStableId)
		{
			for (int32 Z = 0; Z < Height; ++Z)
			{
				FVoxelBlockState EditedTrunk = Trunk;
				InOverlays.ApplyAt(Anchor + FIntVector(0, 0, Z), EditedTrunk);
				if (EditedTrunk != Trunk) return;
			}
			const int32 RootZ = VoxelGeneration::FloorDivide(Anchor.Z - Origin.Z, Step);
			const int32 CrownZ = FMath::Max(RootZ + 1,
				VoxelGeneration::FloorDivide(Anchor.Z + Height - 1 - Origin.Z, Step));
			for (int32 Z = RootZ; Z <= CrownZ; ++Z)
			{
				Draw(FIntVector(Anchor.X, Anchor.Y, Origin.Z + Z * Step), Trunk, false);
			}
			const int32 Radius = FMath::Max(0, (Trees.CrownRadius - 1) / Step);
			for (int32 Y = -Radius; Y <= Radius; ++Y)
			{
				for (int32 X = -Radius; X <= Radius; ++X)
				{
					if (X * X + Y * Y > Radius * Radius) continue;
					Draw(FIntVector(Anchor.X + X * Step, Anchor.Y + Y * Step,
						Origin.Z + CrownZ * Step), Leaves, Radius == 0);
				}
			}
		}, Candidates, Accepted, InCancel);
	if (bSampleFailed) return false;
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	OutError.Reset();
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
