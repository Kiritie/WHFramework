#include "Voxel/Rendering/VoxelProxyBuilder.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"
#include "Voxel/Generation/VoxelGenerationPipeline.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/Ecology/VoxelEcology.h"

namespace
{
	bool IsProxyTreeState(const FVoxelGenerationRuntimeConfig& Config, const uint32 InState)
	{
		const auto Matches = [&](uint16 Symbol)
		{
			FVoxelBlockState State;
			return Config.ToRuntime(Symbol, State) && State.Pack() == InState;
		};
		if (Matches(Config.Recipe->Ecology.TreeTrunk) || Matches(Config.Recipe->Ecology.TreeLeaves)) return true;
		for (const FVoxelTreeSpecies& Species : Config.Recipe->Ecology.TreeSpecies)
		{
			if (Matches(Species.Trunk) || Matches(Species.LeavesDark) || Matches(Species.LeavesMid) || Matches(Species.LeavesLight)) return true;
		}
		return false;
	}

	struct FCoarseOverlayCell
	{
		int32 Removed = 0;
		TMap<uint32, int32> Placed;

		bool Add(const FVoxelBlockState& InState, FVoxelGenerationWorkspace& InWorkspace, FString& OutError)
		{
			if (InState.IsAir())
			{
				++Removed;
			}
			else
			{
				if (!Placed.Contains(InState.Pack()) && !InWorkspace.ReserveMap(Placed, Placed.Num() + 1, OutError)) return false;
				++Placed.FindOrAdd(InState.Pack());
			}
			return true;
		}

		void Apply(const int32 InStep, const bool bArchitecture,
			const FVoxelGenerationRuntimeConfig& InConfig,
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
			const int32 RemovalThreshold = IsProxyTreeState(InConfig, InOutState.Pack()) ? 1 :
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
		const FVoxelGenerationRuntimeConfig& InConfig,
		FVoxelVoxelProxyData& InOutData)
	{
		const int32 Side = InOutData.GridSide;
		for (const auto& Pair : InEdits)
		{
			const FIntVector Local = Pair.Key;
			if (Local.X < 0 || Local.X >= Side || Local.Y < 0 || Local.Y >= Side || Local.Z < 0 || Local.Z >= Side) continue;
			const int32 Index = Local.X + Side * (Local.Y + Side * Local.Z);
			FVoxelBlockState State = InOutData.Cells[Index];
			Pair.Value.Apply(InStep, InArchitecture.Contains(Local), InConfig, State);
			InOutData.Cells[Index] = State;
		}
		for (int32 Face = 0; Face < 6; ++Face)
		{
			if (!InOutData.Known[Face]) continue;
			const int32 Axis = Face / 2;
			for (int32 Y = 0; Y < Side; ++Y)
			{
				for (int32 X = 0; X < Side; ++X)
				{
					FIntVector Local = FIntVector::ZeroValue;
					Local[Axis] = Face % 2 == 0 ? Side : -1;
					Local[(Axis + 1) % 3] = X;
					Local[(Axis + 2) % 3] = Y;
					if (const FCoarseOverlayCell* Edits = InEdits.Find(Local))
					{
							Edits->Apply(InStep, InArchitecture.Contains(Local), InConfig, InOutData.Halo[Face][X + Y * Side]);
					}
				}
			}
		}
	}
}

void FVoxelProxyColumnWorkspace::Reset()
{
	Valid.Empty();
	Columns.Empty();
	Lease.Reset();
	Geometry = {};
	Consumer = {};
	SampledColumns = 0;
	ReusedColumns = 0;
}

uint64 FVoxelVoxelProxyData::GetAllocatedBytes() const
{
	uint64 Bytes =
		Cells.GetAllocatedSize() + TopSurfaceCoverage.GetAllocatedSize();

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
	FVoxelVoxelProxyData& OutData, FString& OutError, const TAtomic<bool>* InCancel,
	const FVoxelTerrainGeometry& InGeometry, const FVoxelTerrainBuildShard* InShard,
	FVoxelProxyColumnWorkspace* InColumns) const
{
	FVoxelGenerationRequiredSourceScope ColumnSources(Cache->GetResources().Get(), InColumns,
		[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			if (InContext) FVoxelGenerationRequiredSourceScope::VisitLease(
				static_cast<const FVoxelProxyColumnWorkspace*>(InContext)->Lease.Get(), InVisit);
		});
	FVoxelVoxelProxyData Data;
	if (!BuildNatural(InKey, Data, OutError, InCancel, InGeometry, InShard, InColumns)) return false;
	if (!ApplyTreeSilhouettes(InKey, InOverlays, Data, OutError, InCancel)) return false;
	const FIntVector Origin = Data.GetBuildBounds().Min;
	const int32 Side = Data.GridSide;
	const int32 Step = Data.Geometry.SampleStepCells;
	FVoxelCoarseOverlayCapture CoarseCapture;
	if (Generator && !Generator->BuildCoarseOverlay(
			Origin, Step, Side, CoarseCapture, OutError, InCancel))
	{
		return false;
	}
	const auto& CoarseNatural = CoarseCapture.Cells;
	const auto& FactResources = Cache->GetResources();
	FVoxelGenerationConsumerKey Consumer;
	if (FactResources && !Cache->ResolveConsumer(Consumer, OutError)) return false;
	FVoxelGenerationRequiredSourceScope CoarseSource(FactResources.Get(), CoarseCapture.Lease.Get());
	FVoxelGenerationWorkspace Workspace(FactResources, Consumer,
		Generator ? Generator->GetRequiredInputBytes() : Cache->GetRootInputBytes(),
		Generator ? Generator->GetRequiredInputLeaseCount() : int32(Cache->GetRootInputBytes() != 0));
	for (const TPair<FIntVector, FVoxelBlockState>& Entry : CoarseNatural)
	{
		const FIntVector& Local = Entry.Key;
		if (Local.X >= 0 && Local.X < Side && Local.Y >= 0 && Local.Y < Side &&
			Local.Z >= 0 && Local.Z < Side)
		{
			Data.Cells[Local.X + Local.Y * Side + Local.Z * Side * Side] = Entry.Value;
		}
		for (int32 Face = 0; Face < 6; ++Face)
		{
			const int32 Axis = Face / 2;
			const int32 U = (Axis + 1) % 3;
			const int32 V = (Axis + 2) % 3;
			if (Data.Known[Face] && Local[Axis] == (Face % 2 == 0 ? Side : -1) &&
				Local[U] >= 0 && Local[U] < Side && Local[V] >= 0 && Local[V] < Side)
			{
				Data.Halo[Face][Local[U] + Local[V] * Side] = Entry.Value;
			}
		}
	}
	TMap<FIntVector, FCoarseOverlayCell> CoarseEdits;
	bool bScratchReady = true;
	InOverlays.Visit([&](const FIntVector& SectionKey, const FVoxelOverlaySnapshot& Section)
	{
		if (!bScratchReady) return;
		const FIntVector SectionOrigin = SectionKey * 16;
		for (const TPair<int32, FVoxelBlockState>& Edit : Section.Blocks)
		{
			const int32 Index = Edit.Key;
			const FIntVector Position = SectionOrigin + FIntVector(
				Index % 16, (Index / 16) % 16, Index / 256);
			const FIntVector Coarse(
				VoxelGeneration::FloorDivide(Position.X - Origin.X, Step),
				VoxelGeneration::FloorDivide(Position.Y - Origin.Y, Step),
				VoxelGeneration::FloorDivide(Position.Z - Origin.Z, Step));
			if (Coarse.X < -1 || Coarse.X > Side || Coarse.Y < -1 || Coarse.Y > Side ||
				Coarse.Z < -1 || Coarse.Z > Side)
			{
				continue;
			}
			if ((!CoarseEdits.Contains(Coarse) && !Workspace.ReserveMap(CoarseEdits, CoarseEdits.Num() + 1, OutError)) ||
				!CoarseEdits.FindOrAdd(Coarse).Add(Edit.Value, Workspace, OutError))
			{
				bScratchReady = false;
				return;
			}
		}
	});
	if (!bScratchReady) return false;
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	FVoxelBlockState Trunk;
	FVoxelBlockState Leaves;
	Config->ToRuntime(Config->Recipe->Ecology.TreeTrunk, Trunk);
	Config->ToRuntime(Config->Recipe->Ecology.TreeLeaves, Leaves);
	ApplyCoarseEdits(CoarseEdits, CoarseNatural, Step, *Config, Data);
	bool bHasAir = false;
	bool bHasSolid = false;
	bHasAir = Data.Cells.ContainsByPredicate([this](const FVoxelBlockState& State)
	{ return State.IsAir() || State == Config->Water || State == Config->Lava; });
	bHasSolid = Data.Cells.ContainsByPredicate([this](const FVoxelBlockState& State)
	{ return !State.IsAir() && State != Config->Water && State != Config->Lava; });
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
	if (InOutData.Geometry.Key != FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::VoxelProxy, InKey.Coordinate, InKey.Level} ||
		InOutData.Key != InKey || InKey.Level == 0 ||
		!FVoxelTerrainBuildShard{InOutData.LocalMinCells, FIntVector(InOutData.GridSide)}.Validate(InOutData.Geometry, 128, OutError) ||
		InOutData.Cells.Num() != int64(InOutData.GridSide) * InOutData.GridSide * InOutData.GridSide)
	{
		OutError = TEXT("Tree silhouette input does not match its actual proxy geometry");
		return false;
	}
	for (int32 Face = 0; Face < 6; ++Face)
	{
		if (InOutData.Known[Face] && InOutData.Halo[Face].Num() != InOutData.GridSide * InOutData.GridSide)
		{
			OutError = TEXT("Tree silhouette input has an invalid proxy halo");
			return false;
		}
	}
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
	const int64 Padding = (FMath::Max<int64>(1, (int64(Trees.CrownRadius) - 1) / InOutData.Geometry.SampleStepCells) + 1) *
		InOutData.Geometry.SampleStepCells;
	FIntVector SamplingMin;
	FIntVector SamplingMax;
	if (Trees.CrownRadius < 0 || Padding > MAX_int32 ||
		!VoxelStreamingGeometry::TrySamplingBounds(InOutData.Geometry, FIntVector(int32(Padding), int32(Padding), 0),
			FIntVector(int32(Padding), int32(Padding), 0), SamplingMin, SamplingMax, OutError))
	{
		OutError = TEXT("Tree silhouette sampling exceeds the supported coordinate range");
		return false;
	}
	auto ClearTreeSamples = [this](TArray<FVoxelBlockState>& Cells)
	{
		for (FVoxelBlockState& Cell : Cells)
		{
			if (IsProxyTreeState(*Config, Cell.Pack())) Cell = FVoxelBlockState();
		}
	};
	InOutData.Cells.Transform([this](FVoxelBlockState& Cell)
	{ if (IsProxyTreeState(*Config, Cell.Pack())) Cell = FVoxelBlockState(); });
	for (TArray<FVoxelBlockState>& Face : InOutData.Halo) ClearTreeSamples(Face);
	FVoxelGenerationQuery Query;
	FVoxelGenerationRequiredSourceScope QuerySources(Cache->GetResources().Get(), &Query,
		[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			static_cast<const FVoxelGenerationQuery*>(InContext)->VisitRequiredSources(InVisit);
		});
	if (!FVoxelGenerationQuery::Create(Config, Cache, Query, OutError, false,
		Generator && Generator->RequiresPlanPreflight()))
	{
		return false;
	}
	const FVoxelGenerationBounds Bounds = InOutData.GetBuildBounds();
	const int32 Side = InOutData.GridSide;
	const int32 Step = InOutData.Geometry.SampleStepCells;
	const FIntVector Origin = Bounds.Min;
	const int32 CrownPadding = int32(Padding);
	const FVoxelGenerationBounds TreeBounds {
		FIntVector(Bounds.Min.X - CrownPadding,
			Bounds.Min.Y - CrownPadding, Config->Recipe->Settings.MinZ),
		FIntVector(Bounds.Max.X + CrownPadding,
			Bounds.Max.Y + CrownPadding, Config->Recipe->Settings.MaxZ)
	};
	bool bSampleFailed = false;
	auto SampleColumn = [&Query, &OutError, &bSampleFailed, InCancel](
		const FIntVector& Position, FVoxelColumnSample& Column)
	{
		if (bSampleFailed) return false;
		if (!Query.SampleEnvironmentColumn(Position.X, Position.Y, Column,
			OutError, InCancel))
		{
			bSampleFailed = true;
			return false;
		}
		return true;
	};
	auto SampleBase = [this, InCancel, &OutError, &bSampleFailed](const FIntVector& Position,
		const FVoxelColumnSample& Column, uint32& OutSymbol)
	{
		if (InCancel && InCancel->Load())
		{
			bSampleFailed = true;
			OutError = TEXT("Canceled");
			return false;
		}
		OutSymbol = Position.Z <= Column.SurfaceZ ?
			Config->Recipe->Palette.Stone : Config->Recipe->Palette.Air;
		return true;
	};
	auto Draw = [&InOutData, &Origin, Step, Side, &Trunk, &Leaves,
		Water = Config->Water](
		const FIntVector& Position, const FVoxelBlockState State,
		const bool bReplaceTrunk)
	{
		const FIntVector Local(
			VoxelGeneration::FloorDivide(Position.X - Origin.X, Step),
			VoxelGeneration::FloorDivide(Position.Y - Origin.Y, Step),
			VoxelGeneration::FloorDivide(Position.Z - Origin.Z, Step));
		if (Local.X >= 0 && Local.X < Side && Local.Y >= 0 && Local.Y < Side &&
			Local.Z >= 0 && Local.Z < Side)
		{
			const int32 Index = Local.X + Local.Y * Side + Local.Z * Side * Side;
			const FVoxelBlockState Cell = InOutData.Cells[Index];
			if (Cell.IsAir() || Cell == Water || (State == Trunk && Cell == Leaves) ||
				(bReplaceTrunk && Cell == Trunk)) InOutData.Cells[Index] = State;
		}
		for (int32 Face = 0; Face < 6; ++Face)
		{
			const int32 Axis = Face / 2;
			const int32 U = (Axis + 1) % 3;
			const int32 V = (Axis + 2) % 3;
			if (InOutData.Known[Face] && Local[Axis] == (Face % 2 == 0 ? Side : -1) &&
				Local[U] >= 0 && Local[U] < Side && Local[V] >= 0 && Local[V] < Side)
			{
				FVoxelBlockState& Cell = InOutData.Halo[Face][Local[U] + Local[V] * Side];
				if (Cell.IsAir() || Cell == Water || (State == Trunk && Cell == Leaves) ||
					(bReplaceTrunk && Cell == Trunk)) Cell = State;
			}
		}
	};
	const FVoxelEcologyGenerator Ecology(Config->Recipe.ToSharedRef());
	int32 Candidates = 0;
	int32 Accepted = 0;
	Ecology.EnumerateTrees(TreeBounds, SampleColumn, SampleBase,
		[&](const FIntVector& Anchor, const int32 Height, const FVoxelStableId TreeId, const FVoxelColumnSample& Column)
		{
			const FVoxelTreeSpecies Species = Ecology.SelectTreeSpecies(Column, TreeId);
			if (!Config->ToRuntime(Species.Trunk, Trunk) || !Config->ToRuntime(Species.LeavesMid, Leaves))
			{
				OutError = TEXT("InvalidResource: proxy tree species symbols are invalid");
				bSampleFailed = true;
				return;
			}
			if (InOverlays.Num())
				for (int32 Z = 0; Z < Height; ++Z)
				{
					const FIntRect Section = FVoxelEcologyGenerator::TrunkSection(Height, Z, Trees.CrownRadius, TreeId, Species.Form);
					for (int32 Y = Section.Min.Y; Y < Section.Max.Y; ++Y)
						for (int32 X = Section.Min.X; X < Section.Max.X; ++X)
						{
							FVoxelBlockState EditedTrunk = Trunk;
							InOverlays.ApplyAt(Anchor + FIntVector(X, Y, Z), EditedTrunk);
							if (EditedTrunk != Trunk) return;
						}
				}
			const int32 RootZ = VoxelGeneration::FloorDivide(Anchor.Z - Origin.Z, Step);
			const int32 LastTrunkZ = VoxelGeneration::FloorDivide(Anchor.Z + Height - 1 - Origin.Z, Step);
			for (int32 Z = RootZ; Z <= LastTrunkZ; ++Z)
			{
				const int32 FirstLayer = FMath::Max(0, Origin.Z + Z * Step - Anchor.Z);
				const int32 LastLayer = FMath::Min(Height, Origin.Z + (Z + 1) * Step - Anchor.Z);
				if (FirstLayer >= LastLayer) continue;
				const FIntRect Section = FVoxelEcologyGenerator::TrunkBounds(Height, FirstLayer, LastLayer, Trees.CrownRadius, TreeId, Species.Form);
				const int32 MinX = VoxelGeneration::FloorDivide(Anchor.X + Section.Min.X - Origin.X, Step);
				const int32 MinY = VoxelGeneration::FloorDivide(Anchor.Y + Section.Min.Y - Origin.Y, Step);
				const int32 MaxX = VoxelGeneration::FloorDivide(Anchor.X + Section.Max.X - 1 - Origin.X, Step);
				const int32 MaxY = VoxelGeneration::FloorDivide(Anchor.Y + Section.Max.Y - 1 - Origin.Y, Step);
				for (int32 Y = MinY; Y <= MaxY; ++Y)
					for (int32 X = MinX; X <= MaxX; ++X)
						Draw(Origin + FIntVector(X, Y, Z) * Step, Trunk, false);
			}
			const int32 Radius = Trees.CrownRadius;
			const FIntVector CrownCenter = Anchor + FIntVector(0, 0, Height - 1);
			FIntVector ShapeMin;
			FIntVector ShapeMax;
			FVoxelEcologyGenerator::CrownBounds(Radius, Species.Form, ShapeMin, ShapeMax);
			FIntVector CrownMin;
			FIntVector CrownMax;
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				CrownMin[Axis] = VoxelGeneration::FloorDivide(CrownCenter[Axis] + ShapeMin[Axis] - Origin[Axis], Step);
				CrownMax[Axis] = VoxelGeneration::FloorDivide(CrownCenter[Axis] + ShapeMax[Axis] - 1 - Origin[Axis], Step);
			}
			for (int32 Z = CrownMin.Z; Z <= CrownMax.Z; ++Z)
				for (int32 Y = CrownMin.Y; Y <= CrownMax.Y; ++Y)
					for (int32 X = CrownMin.X; X <= CrownMax.X; ++X)
					{
						const FIntVector Min = Origin + FIntVector(X, Y, Z) * Step;
						if (!FVoxelEcologyGenerator::CrownIntersects(Min - CrownCenter, Min + FIntVector(Step) - CrownCenter, Radius, TreeId, Species.Form)) continue;
						const int32 LeafZ = FMath::Clamp(Min.Z + Step / 2 - CrownCenter.Z, -Radius, Radius);
						Config->ToRuntime(FVoxelEcologyGenerator::SelectLeaf(Species, LeafZ, Radius), Leaves);
						Draw(Min, Leaves, true);
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
	const TAtomic<bool>* InCancel,
	const FVoxelTerrainGeometry& InGeometry, const FVoxelTerrainBuildShard* InShard,
	FVoxelProxyColumnWorkspace* InColumns) const
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

	const FVoxelTerrainGeometry Geometry = InGeometry;
	if (!Geometry.Validate(OutError)) return false;
	if (Geometry.Key.Representation != EVoxelTerrainRepresentation::VoxelProxy ||
		Geometry.Key.Coordinate != InKey.Coordinate || Geometry.Key.Level != InKey.Level)
	{ OutError = TEXT("Proxy spatial key does not match its required geometry"); return false; }
	const int32 GridSide = InShard ? InShard->CellCounts.X : int32(Geometry.Key.SideCells()) / Geometry.SampleStepCells;
	const FVoxelTerrainBuildShard Window = InShard ? *InShard :
		FVoxelTerrainBuildShard{FIntVector::ZeroValue, FIntVector(GridSide)};
	if (!Window.Validate(Geometry, 128, OutError))
	{
		return false;
	}

	const int32 ColumnSampleSide =
		GridSide + 2;

	const int32 Step = Geometry.SampleStepCells;

	const FIntVector WindowMin = InKey.GetBounds().Min + (InShard ? InShard->LocalMinCells : FIntVector::ZeroValue);
	const FVoxelGenerationBounds Bounds{WindowMin, WindowMin + FIntVector(GridSide * Step)};
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (int64(Bounds.Min[Axis]) - Step < MIN_int32 || int64(Bounds.Max[Axis]) + Step > MAX_int32)
		{
			OutError = TEXT("Proxy sampling halo exceeds the supported coordinate range");
			return false;
		}
	}

	const auto& FactResources = Cache->GetResources();
	FVoxelGenerationConsumerKey Consumer;
	if (FactResources && !Cache->ResolveConsumer(Consumer, OutError)) return false;
	const int32 PageSide = int32(Geometry.Key.SideCells() / Step);
	const int32 ColumnStride = InColumns ? PageSide + 2 : ColumnSampleSide;
	const FIntPoint ColumnOffset = InColumns && InShard ?
		FIntPoint(InShard->LocalMinCells.X / Step, InShard->LocalMinCells.Y / Step) : FIntPoint::ZeroValue;
	if (InColumns && (PageSide <= 0 || PageSide > 256 ||
		(!InColumns->Columns.IsEmpty() && (InColumns->Geometry != Geometry || InColumns->Consumer != Consumer ||
			(FactResources && (!InColumns->Lease || !InColumns->Lease->IsOwnedBy(*FactResources))) ||
			InColumns->Columns.Num() != ColumnStride * ColumnStride || InColumns->Valid.Num() != InColumns->Columns.Num()))))
	{
		OutError = TEXT("InvalidResource: proxy column workspace identity or capacity");
		return false;
	}
	const auto ColumnIndex = [ColumnStride, ColumnOffset](const int32 InLocalX, const int32 InLocalY)
	{
		return ColumnOffset.X + InLocalX + 1 + (ColumnOffset.Y + InLocalY + 1) * ColumnStride;
	};
	bool bColumnsReady = InColumns && !InColumns->Columns.IsEmpty();
	for (int32 Y = -1; Y <= GridSide && bColumnsReady; ++Y)
		for (int32 X = -1; X <= GridSide && bColumnsReady; ++X)
			bColumnsReady = InColumns->Valid[ColumnIndex(X, Y)] != 0;
	FVoxelGenerationRequiredSourceScope ColumnSources(FactResources.Get(), InColumns,
		[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			if (InContext) FVoxelGenerationRequiredSourceScope::VisitLease(
				static_cast<const FVoxelProxyColumnWorkspace*>(InContext)->Lease.Get(), InVisit);
		});
	FVoxelGenerationQuery Query;
	FVoxelGenerationRequiredSourceScope QuerySources(Cache->GetResources().Get(), &Query,
		[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			static_cast<const FVoxelGenerationQuery*>(InContext)->VisitRequiredSources(InVisit);
		});

	if (!FVoxelGenerationQuery::Create(
			Config,
			Cache,
			Query,
			OutError,
			false,
			Generator && Generator->RequiresPlanPreflight()))
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

	if (!bColumnsReady && !Query.PrepareColumns(
			ColumnBounds,
			OutError,
			InCancel))
	{
		return false;
	}

	FVoxelGenerationWorkspace ColumnsWorkspace(FactResources, Consumer,
		Query.GetRequiredInputBytes(), Query.GetRequiredInputLeaseCount());
	TArray<FVoxelColumnSample> LocalColumns;
	TArray<uint8> LocalValid;
	if (!InColumns || InColumns->Columns.IsEmpty())
	{
		const int32 Count = ColumnStride * ColumnStride;
		if (!ColumnsWorkspace.ReserveArray(LocalColumns, Count, OutError) ||
			(InColumns && !ColumnsWorkspace.ReserveArray(LocalValid, Count, OutError))) return false;
		LocalColumns.SetNumUninitialized(Count);
		if (InColumns)
		{
			LocalValid.Init(0, Count);
			InColumns->Columns = MoveTemp(LocalColumns);
			InColumns->Valid = MoveTemp(LocalValid);
			InColumns->Lease = ColumnsWorkspace.TakeLease();
			InColumns->Geometry = Geometry;
			InColumns->Consumer = Consumer;
		}
	}
	TArrayView<FVoxelColumnSample> Columns = InColumns ? MakeArrayView(InColumns->Columns) : MakeArrayView(LocalColumns);

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
			const int32 Index = ColumnIndex(LocalX, LocalY);
			if (InColumns && InColumns->Valid[Index])
			{
				++InColumns->ReusedColumns;
				continue;
			}
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

			if (!Query.SampleEnvironmentColumn(
					WorldX,
					WorldY,
					Columns[Index],
					OutError, InCancel))
			{
				return false;
			}
			if (InColumns)
			{
				InColumns->Valid[Index] = 1;
				++InColumns->SampledColumns;
			}
		}
	}

	auto ResolveCell =
		[
			this,
			&Bounds,
			Step,
			GridSide,
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

	Data.Geometry = Geometry;
	Data.LocalMinCells = InShard ? InShard->LocalMinCells : FIntVector::ZeroValue;
	Data.GridSide = GridSide;

	Data.Cells.InitializeColumns(GridSide, Config->Stone);
	for (int32 Y = 0; Y < GridSide; ++Y)
	{
		if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return false; }
		for (int32 X = 0; X < GridSide; ++X)
		{
			const auto& Sample = Columns[ColumnIndex(X, Y)];
			FVoxelProxyColumn Column;
			Column.Ground = int32(FMath::Clamp<int64>(VoxelStreamingGeometry::FloorDivide(int64(Sample.SurfaceZ) - Bounds.Min.Z, Step), MIN_int32, MAX_int32));
			Column.Water = Sample.SurfaceWaterZ == MIN_int32 ? MIN_int32 : int32(FMath::Clamp<int64>(VoxelStreamingGeometry::FloorDivide(int64(Sample.SurfaceWaterZ) - Bounds.Min.Z, Step), MIN_int32, MAX_int32));
			Column.WaterState = Config->Water;
			if (!Config->ToRuntime(Sample.SurfaceMaterial, Column.Surface))
			{ OutError = TEXT("VoxelProxy surface symbol is invalid"); return false; }
			Data.Cells.SetColumn(X + Y * GridSide, Column);
		}
	}
	const bool bHasAir = Data.Cells.ContainsByPredicate([](const FVoxelBlockState& State) { return State.IsAir(); });
	const bool bHasSolid = Data.Cells.ContainsByPredicate([this](const FVoxelBlockState& State)
	{ return !State.IsAir() && State != Config->Water && State != Config->Lava; });

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
