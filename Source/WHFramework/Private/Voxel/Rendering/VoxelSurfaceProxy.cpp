#include "Voxel/Rendering/VoxelSurfaceProxy.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationPlanningRead.h"
#include "Voxel/Generation/Ecology/VoxelEcology.h"

namespace
{
	constexpr int32 SectionSide = 16;

	FIntPoint ResolveWorldXY(
		const FIntPoint& InOrigin,
		const int32 InX,
		const int32 InY,
		const int32 InStep, const int32 InGridSide)
	{
		(void)InGridSide;

		return
			InOrigin +
			FIntPoint(
				InX *
					InStep,
				InY *
					InStep);
	}

	FIntVector UnpackCell(
		const int32 InIndex)
	{
		return FIntVector(
			InIndex %
				SectionSide,
			(InIndex /
				SectionSide) %
				SectionSide,
			InIndex /
				(
					SectionSide *
					SectionSide
				));
	}

	FVoxelGenerationBounds DistantInfluenceBounds(
		const FVoxelSurfaceTileData& InData,
		const FVoxelGenerationSettings& InSettings,
		const FVoxelTreeGenerationSettings& InTrees)
	{
		const int32 TileSide = InData.GetTileSide();
		const FIntPoint Origin = InData.GetBuildOrigin();
		const int32 Crown = InTrees.bEnabled
			? FMath::Max(InData.Step, InTrees.CrownRadius) : InData.Step;
		return {
			FIntVector(Origin.X - Crown, Origin.Y - Crown, InSettings.MinZ),
			FIntVector(Origin.X + TileSide + Crown,
				Origin.Y + TileSide + Crown, InSettings.MaxZ)
		};
	}
}

FVoxelSurfaceProxyBuilder::FVoxelSurfaceProxyBuilder(
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator,
	TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
	const FVoxelGenerationSettings& InSettings,
	const IVoxelOverlaySource& InOverlaySource,
	TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry,
	const FVoxelLodTopologySettings& InLod)
	: Lod(InLod)
	, Generator(InGenerator)
	, Config(InConfig)
	, Settings(InSettings)
	, OverlaySource(InOverlaySource)
	, Registry(InRegistry)
{
}

bool FVoxelSurfaceProxyBuilder::Build(
	const FVoxelSurfaceTileKey& InKey,
	FVoxelSurfaceTileData& OutData,
	FString& OutError,
	const TAtomic<bool>* InCancel,
	FVoxelSurfaceBuildTiming* OutTiming,
	TArray<FVoxelColumnSample>* OutColumns,
	const FVoxelTerrainGeometry& InGeometry,
	const FVoxelTerrainBuildShard* InShard,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>* OutColumnsLease) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_SurfaceBuild);

	const auto& Cache = Generator->GetPlanCache();
	const auto& Resources = Cache->GetResources();
	FVoxelGenerationRequiredSourceScope RootSources(Resources.Get(), &Generator.Get(),
		[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			static_cast<const FVoxelGenerationPipeline*>(InContext)->VisitRequiredSources(InVisit);
		});
	FVoxelGenerationRequiredSourceScope OldColumnsSources(Resources.Get(),
		OutColumnsLease && *OutColumnsLease ? OutColumnsLease->Get() : nullptr);
	FVoxelGenerationConsumerKey Consumer;
	if (Resources)
	{
		if (OutColumns && !OutColumnsLease)
		{
			OutError = TEXT("InvalidResource: surface column output requires its original strong lease slot");
			return false;
		}
		if (!FVoxelGenerationConsumerScope::GetConsumerKey() &&
			!FVoxelGenerationPlanningReadScope::GetConsumerKey() &&
			!FVoxelGenerationPlanningReadScope::GetTaskStamp())
		{
			OutError = TEXT("InvalidConsumer: surface columns require their original execution consumer");
			return false;
		}
		if (!Cache->ResolveConsumer(Consumer, OutError))
		{
			return false;
		}
	}

	FVoxelSurfaceTileData Data;

	Data.Key =
		InKey;

	Data.Geometry = InGeometry;
	if (!Data.Geometry.Validate(OutError)) return false;
	if ((Data.Geometry.Key.Representation != EVoxelTerrainRepresentation::Surface && Data.Geometry.Key.Representation != EVoxelTerrainRepresentation::Macro) ||
		Data.Geometry.Key.Coordinate != FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0) || Data.Geometry.Key.Level != InKey.Level)
	{ OutError = TEXT("Surface spatial key does not match its required geometry"); return false; }
	const int32 SampleSide = InShard ? InShard->CellCounts.X : int32(Data.Geometry.Key.SideCells()) / Data.Geometry.SampleStepCells;
	const FVoxelTerrainBuildShard Window = InShard ? *InShard :
		FVoxelTerrainBuildShard{FIntVector::ZeroValue, FIntVector(SampleSide, SampleSide, 0)};
	if (!Window.Validate(Data.Geometry, 256, OutError))
	{
		return false;
	}
	Data.Step = Data.Geometry.SampleStepCells;
	Data.LocalMinCells = InShard ? FIntPoint(InShard->LocalMinCells.X, InShard->LocalMinCells.Y) : FIntPoint::ZeroValue;
	Data.Side = SampleSide + 1;

	const FVoxelTreeGenerationSettings& Trees = Config->Recipe->Settings.Ecology.Tree;
	const int32 Padding = Trees.bEnabled ? FMath::Max(Data.Step, Trees.CrownRadius) : Data.Step;
	FIntVector SamplingMin;
	FIntVector SamplingMax;
	if (Settings.MinZ >= Settings.MaxZ || Trees.CrownRadius < 0 ||
		!VoxelStreamingGeometry::TrySamplingBounds(Data.Geometry, FIntVector(Padding, Padding, 0),
			FIntVector(Padding, Padding, 0), SamplingMin, SamplingMax, OutError))
	{
		OutError = TEXT("Surface sampling bounds exceed the supported coordinate range");
		return false;
	}
	const int32 Count =
		Data.GetVertexCount();

	Data.GroundZ.SetNumUninitialized(Count);
	Data.WaterZ.SetNumUninitialized(Count);
	Data.SurfaceStates.SetNumUninitialized(Count);
	Data.Biome.SetNumUninitialized(Count);
	Data.Flags.Init(0, Count);


	const FIntPoint TileOrigin = Data.GetBuildOrigin();

	// 列 scratch 独立于现有 Terrain Geometry 账本；数组先释放，再退出原事实 Working 租约。
	FVoxelGenerationWorkspace ColumnsWorkspace(Resources, Consumer,
		Generator->GetRequiredInputBytes(), Generator->GetRequiredInputLeaseCount());
	TArray<FVoxelColumnSample> Columns;
	if (!ColumnsWorkspace.ReserveArray(Columns, Count, OutError))
	{
		return false;
	}
	Columns.SetNumUninitialized(Count);
	const double ColumnsStart = FPlatformTime::Seconds();

	if (!Generator->SampleColumns(
		TileOrigin,
		Data.Side,
		Data.Side,
		Data.Step,
		MakeArrayView(Columns),
		OutError,
		InCancel,
		false,
		ColumnsWorkspace.GetLease()))
	{
		return false;
	}
	const double ColumnsMilliseconds =
		(FPlatformTime::Seconds() - ColumnsStart) * 1000.0;

	if (Columns.Num() !=
		Count)
	{
		OutError =
			TEXT("Voxel surface column grid returned an invalid vertex count");

		return false;
	}

	for (int32 Index = 0;
		Index < Count;
		++Index)
	{
		const FVoxelColumnSample& Column =
			Columns[Index];

		Data.GroundZ[Index] =
			Column.SurfaceZ;

		Data.WaterZ[Index] =
			Column.SurfaceWaterZ;

		if (!Config->ToRuntime(Column.SurfaceMaterial, Data.SurfaceStates[Index]) ||
			!Registry->Find(Data.SurfaceStates[Index].TypeId))
		{
			OutError = TEXT("Natural surface material cannot resolve to the runtime registry");
			return false;
		}

		Data.Biome[Index] =
			Column.BiomeIndex;

		uint8 Flags = 0;

		if (Column.bRiver)
		{
			Flags |= VoxelSurface_River;
		}

		if (Column.bLake)
		{
			Flags |= VoxelSurface_Lake;
		}

		if (Column.bOcean)
		{
			Flags |= VoxelSurface_Ocean;
		}

		if (Column.bCoast)
		{
			Flags |= VoxelSurface_Coast;
		}

		Data.Flags[Index] =
			Flags;
	}

	const double OverlayStart = FPlatformTime::Seconds();
	FVoxelGenerationWorkspace ModifiedWorkspace(Resources, Consumer,
		Generator->GetRequiredInputBytes(), Generator->GetRequiredInputLeaseCount());
	TMap<FIntVector, FVoxelBlockState> ModifiedBlocks;
	if (!CaptureModifiedBlocks(Data, ModifiedBlocks, Data.Revision, OutError, InCancel, ModifiedWorkspace))
	{
		return false;
	}
	if (!ApplyModifiedSurface(
		InKey,
		Data,
		ModifiedBlocks,
		OutError,
		InCancel))
	{
		return false;
	}
	if (!BuildDistantCells(Data, ModifiedBlocks, OutError, InCancel))
	{
		return false;
	}
	const double OverlayMilliseconds =
		(FPlatformTime::Seconds() - OverlayStart) * 1000.0;
	if (OutTiming)
	{
		OutTiming->ColumnsMilliseconds = ColumnsMilliseconds;
		OutTiming->OverlayMilliseconds = OverlayMilliseconds;
	}

	if (OutColumns)
	{
		*OutColumns = MoveTemp(Columns);
		if (OutColumnsLease)
		{
			*OutColumnsLease = ColumnsWorkspace.TakeLease();
		}
	}
	OutData =
		MoveTemp(Data);

	OutError.Reset();
	return true;
}

bool FVoxelSurfaceProxyBuilder::CaptureModifiedBlocks(
	const FVoxelSurfaceTileData& InData,
	TMap<FIntVector, FVoxelBlockState>& OutBlocks,
	uint64& OutRevision,
	FString& OutError,
	const TAtomic<bool>* InCancel,
	FVoxelGenerationWorkspace& InWorkspace) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_SurfaceCaptureModified);

	const FVoxelTreeGenerationSettings& Trees = Config->Recipe->Settings.Ecology.Tree;
	const FVoxelGenerationBounds Bounds = DistantInfluenceBounds(InData, Settings, Trees);
	int64 Count = 0;
	if (!OverlaySource.VisitModifiedOverlays(Bounds,
		[&](const FIntVector&, const FVoxelOverlaySnapshot& Snapshot)
		{
			Count += Snapshot.Blocks.Num();
			if (Count <= MAX_int32) return true;
			OutError = TEXT("Oversized: surface modified block count");
			return false;
		}, OutError, InCancel) || !InWorkspace.ReserveMap(OutBlocks, int32(Count), OutError))
	{
		return false;
	}
	uint64 Revision = OutRevision;
	if (!OverlaySource.VisitModifiedOverlays(Bounds,
		[&](const FIntVector& Section, const FVoxelOverlaySnapshot& Snapshot)
		{
			Revision = FMath::Max(Revision, Snapshot.Revision);
			for (const TPair<int32, FVoxelBlockState>& Edit : Snapshot.Blocks)
			{
				if (Edit.Key < 0 || Edit.Key >= SectionSide * SectionSide * SectionSide)
				{
					OutError = TEXT("Distant voxel overlay contains an invalid cell index");
					return false;
				}
				const FIntVector Local = UnpackCell(Edit.Key);
				FIntVector Cell;
				if (!VoxelGeneration::TryWorldCell(FInt64Vector(
					int64(Section.X) * SectionSide + Local.X,
					int64(Section.Y) * SectionSide + Local.Y,
					int64(Section.Z) * SectionSide + Local.Z), Cell)) continue;
				if (!OutBlocks.Contains(Cell) && !InWorkspace.ReserveMap(OutBlocks, OutBlocks.Num() + 1, OutError)) return false;
				OutBlocks.Add(Cell, Edit.Value);
			}
			return true;
		}, OutError, InCancel)) return false;
	OutRevision = Revision;
	OutError.Reset();
	return true;
}

bool FVoxelSurfaceProxyBuilder::BuildDistantCells(
	FVoxelSurfaceTileData& InOutData,
	const TMap<FIntVector, FVoxelBlockState>& InModifiedBlocks,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_SurfaceDistantCells);

	const int32 Step = InOutData.Step;
	const int32 Side = InOutData.GetTileSide();
	const FIntPoint Origin = InOutData.GetBuildOrigin();
	const FIntPoint End = Origin + FIntPoint(Side, Side);
	const FVoxelTreeGenerationSettings& Trees = Config->Recipe->Settings.Ecology.Tree;
	const FVoxelGenerationBounds Bounds = DistantInfluenceBounds(InOutData, Settings, Trees);

	int32 MinGround = MAX_int32;
	int32 MaxGround = MIN_int32;
	for (const int32 Ground : InOutData.GroundZ)
	{
		if (Ground != MIN_int32)
		{
			MinGround = FMath::Min(MinGround, Ground);
			MaxGround = FMath::Max(MaxGround, Ground);
		}
	}
	if (MinGround == MAX_int32)
	{
		MinGround = 0;
		MaxGround = 0;
	}
	const int32 StructureStep = FMath::Min(Step, 8);
	const int32 StructureSide = Side / StructureStep;
	const auto& FactResources = Generator->GetPlanCache()->GetResources();
	FVoxelGenerationRequiredSourceScope RootSources(FactResources.Get(), &Generator.Get(),
		[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			static_cast<const FVoxelGenerationPipeline*>(InContext)->VisitRequiredSources(InVisit);
		});
	FVoxelGenerationConsumerKey CoarseConsumer;
	if (FactResources && !Generator->GetPlanCache()->ResolveConsumer(CoarseConsumer, OutError)) return false;
	FVoxelGenerationWorkspace StructureWorkspace(FactResources, CoarseConsumer,
		Generator->GetRequiredInputBytes(), Generator->GetRequiredInputLeaseCount());
	TMap<FIntVector, FVoxelBlockState> StructureCells;
	const int32 FirstZ = VoxelGeneration::FloorDivide(
		FMath::Max(Settings.MinZ, MinGround - 16), StructureStep) * StructureStep;
	const int32 LastZ = FMath::Min(Settings.MaxZ - 1, MaxGround + 64);
	for (int64 BaseZ = FirstZ; BaseZ <= LastZ;
		BaseZ += static_cast<int64>(StructureSide) * StructureStep)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_SurfaceStructureBand);
		FVoxelCoarseOverlayCapture Band;
		if (!Generator->BuildCoarseOverlay(FIntVector(Origin.X, Origin.Y,
			static_cast<int32>(BaseZ)), StructureStep, StructureSide,
			Band, OutError, InCancel))
		{
			return false;
		}
		StructureWorkspace.SetRequirements(Generator->GetRequiredInputBytes(),
			Generator->GetRequiredInputLeaseCount() + (Band.Lease ? 1 : 0), Band.Lease ? Band.Lease->Bytes() : 0);
		FVoxelGenerationRequiredSourceScope BandSource(FactResources.Get(), Band.Lease.Get());
		for (const TPair<FIntVector, FVoxelBlockState>& Entry : Band.Cells)
		{
			if (Entry.Key.X >= 0 && Entry.Key.X < StructureSide &&
				Entry.Key.Y >= 0 && Entry.Key.Y < StructureSide &&
				Entry.Key.Z >= 0 && Entry.Key.Z < StructureSide)
			{
				const FIntVector Key(Entry.Key.X, Entry.Key.Y,
					VoxelGeneration::FloorDivide(static_cast<int32>(BaseZ) + Entry.Key.Z * StructureStep, StructureStep));
				if (!StructureCells.Contains(Key) && !StructureWorkspace.ReserveMap(StructureCells, StructureCells.Num() + 1, OutError)) return false;
				StructureCells.Add(Key, Entry.Value);
			}
		}
		StructureWorkspace.SetRequirements(Generator->GetRequiredInputBytes(), Generator->GetRequiredInputLeaseCount(), 0);
	}

	auto AddCell = [&](FIntVector Min, FIntVector Max, const FVoxelBlockState State,
		const FVoxelStableId SourceTreeId = {})
	{
		Min.X = FMath::Max(Min.X, Origin.X);
		Min.Y = FMath::Max(Min.Y, Origin.Y);
		Max.X = FMath::Min(Max.X, End.X);
		Max.Y = FMath::Min(Max.Y, End.Y);
		if (!State.IsAir() && Min.X < Max.X && Min.Y < Max.Y && Min.Z < Max.Z)
		{
			InOutData.DistantCells.Add({Min, Max, State, SourceTreeId});
		}
	};
	if (Trees.bEnabled && FMath::Max(Trees.MaxHeight, Trees.CrownRadius * 2 + 1) >= Step)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_SurfaceDistantTrees);
		FVoxelBlockState Trunk;
		FVoxelBlockState Leaves;
		if (!Config->ToRuntime(Config->Recipe->Ecology.TreeTrunk, Trunk) ||
			!Config->ToRuntime(Config->Recipe->Ecology.TreeLeaves, Leaves))
		{
			OutError = TEXT("Distant tree materials are invalid");
			return false;
		}
		FString TreeSampleError;
		bool bTreeSampleFailed = false;
		const bool bTreesEnumerated = Generator->EnumerateTrees(Bounds,
			[&](const FIntVector& Anchor, const int32 Height,
				const FVoxelStableId TreeId)
			{
				if (bTreeSampleFailed) return;
				FVoxelColumnSample Column;
				if (!Generator->SampleColumn(Anchor.X, Anchor.Y, Column, TreeSampleError, InCancel))
				{
					bTreeSampleFailed = true;
					return;
				}
				const FVoxelEcologyGenerator Ecology(Config->Recipe.ToSharedRef());
				const FVoxelTreeSpecies Species = Ecology.SelectTreeSpecies(Column, TreeId);
				if (!Config->ToRuntime(Species.Trunk, Trunk) || !Config->ToRuntime(Species.LeavesMid, Leaves)) return;
				if (FMath::Max(Height, Trees.CrownRadius * 2 + 1) < Step) return;
				const FIntVector CoarseAnchor(
					VoxelGeneration::FloorDivide(Anchor.X - Origin.X, StructureStep),
					VoxelGeneration::FloorDivide(Anchor.Y - Origin.Y, StructureStep),
					VoxelGeneration::FloorDivide(Anchor.Z, StructureStep));
				if (StructureCells.Contains(CoarseAnchor)) return;
				for (int32 Z = 0; Z < Height; ++Z)
				{
					if (const FVoxelBlockState* Edit = InModifiedBlocks.Find(Anchor + FIntVector(0, 0, Z)))
					{
						if (*Edit != Trunk) return;
					}
				}
				const int32 Radius = Trees.CrownRadius;
				const FIntVector CrownCenter = Anchor + FIntVector(0, 0, Height - 1);
				for (const TPair<FIntVector, FVoxelBlockState>& Edit : InModifiedBlocks)
				{
					const int64 DX = static_cast<int64>(Edit.Key.X) - CrownCenter.X;
					const int64 DY = static_cast<int64>(Edit.Key.Y) - CrownCenter.Y;
					const int64 DZ = static_cast<int64>(Edit.Key.Z) - CrownCenter.Z;
					if (FMath::Abs(DX) <= Radius && FMath::Abs(DY) <= Radius &&
						FMath::Abs(DZ) <= Radius &&
						FVoxelEcologyGenerator::ContainsCrown(FIntVector(DX, DY, DZ), Radius, TreeId, Species.Form))
					{
						FVoxelBlockState ExpectedLeaf;
						Config->ToRuntime(FVoxelEcologyGenerator::SelectLeaf(Species, static_cast<int32>(DZ), Radius), ExpectedLeaf);
						if (Edit.Value != ExpectedLeaf && Edit.Value != Trunk) return;
					}
				}
				AddCell(Anchor, Anchor + FIntVector(1, 1, Height), Trunk, TreeId);
				const int32 CrownStep = FMath::Max(1, Radius / 3);
				for (int32 Z = -Radius; Z <= Radius; Z += CrownStep)
				{
					for (int32 Y = -Radius; Y <= Radius; Y += CrownStep)
					{
						for (int32 X = -Radius; X <= Radius; X += CrownStep)
						{
							const FIntVector Offset(X, Y, Z);
							if (!FVoxelEcologyGenerator::ContainsCrown(Offset, Radius, TreeId, Species.Form)) continue;
							Config->ToRuntime(FVoxelEcologyGenerator::SelectLeaf(Species, Z, Radius), Leaves);
							const FIntVector Min = CrownCenter + Offset;
							const FIntVector Max = Min + FIntVector(FMath::Min(CrownStep, Radius + 1 - X),
								FMath::Min(CrownStep, Radius + 1 - Y), FMath::Min(CrownStep, Radius + 1 - Z));
							AddCell(Min, Max, Leaves, TreeId);
						}
					}
				}
			}, OutError, InCancel);
		if (bTreeSampleFailed)
		{
			OutError = MoveTemp(TreeSampleError);
			return false;
		}
		if (!bTreesEnumerated) return false;
	}

	struct FEditVote
	{
		int32 Removed = 0;
		TMap<uint32, int32> Placed;
		FIntVector Min = FIntVector(MAX_int32);
		FIntVector Max = FIntVector(MIN_int32);
		int32 Count = 0;
	};
	TMap<FIntVector, FEditVote> Votes;
	TSet<FIntVector> VisitedEdits;
	const FIntVector EditNeighbors[] = {
		FIntVector(1, 0, 0), FIntVector(-1, 0, 0),
		FIntVector(0, 1, 0), FIntVector(0, -1, 0),
		FIntVector(0, 0, 1), FIntVector(0, 0, -1)
	};
	auto EditClass = [this](const FVoxelBlockState& State)
	{
		if (State.IsAir()) return 1;
		const FVoxelRuntimeDefinition* Definition = Registry->Find(State.TypeId);
		return Definition && Definition->bSolid ? 2 : 0;
	};
	for (const TPair<FIntVector, FVoxelBlockState>& Edit : InModifiedBlocks)
	{
		const int32 Class = EditClass(Edit.Value);
		if (Class == 0 || VisitedEdits.Contains(Edit.Key)) continue;
		FVoxelGenerationWorkspace ComponentWorkspace(FactResources, CoarseConsumer,
			Generator->GetRequiredInputBytes(), Generator->GetRequiredInputLeaseCount());
		TArray<FIntVector> Component;
		if (!ComponentWorkspace.ReserveArray(Component, 1, OutError) ||
			!StructureWorkspace.ReserveSet(VisitedEdits, VisitedEdits.Num() + 1, OutError)) return false;
		Component.Add(Edit.Key);
		VisitedEdits.Add(Edit.Key);
		FIntVector Min = Edit.Key;
		FIntVector Max = Edit.Key;
		for (int32 Index = 0; Index < Component.Num(); ++Index)
		{
			if (InCancel && InCancel->Load())
			{
				OutError = TEXT("Canceled");
				return false;
			}
			const FIntVector Cell = Component[Index];
			Min.X = FMath::Min(Min.X, Cell.X);
			Min.Y = FMath::Min(Min.Y, Cell.Y);
			Min.Z = FMath::Min(Min.Z, Cell.Z);
			Max.X = FMath::Max(Max.X, Cell.X);
			Max.Y = FMath::Max(Max.Y, Cell.Y);
			Max.Z = FMath::Max(Max.Z, Cell.Z);
			for (const FIntVector& Offset : EditNeighbors)
			{
				const FIntVector Neighbor = Cell + Offset;
				const FVoxelBlockState* NeighborState = InModifiedBlocks.Find(Neighbor);
				if (NeighborState && EditClass(*NeighborState) == Class &&
					!VisitedEdits.Contains(Neighbor))
				{
					if (!StructureWorkspace.ReserveSet(VisitedEdits, VisitedEdits.Num() + 1, OutError) ||
						!ComponentWorkspace.ReserveArray(Component, Component.Num() + 1, OutError)) return false;
					VisitedEdits.Add(Neighbor);
					Component.Add(Neighbor);
				}
			}
		}
		const int32 Span = FMath::Max3(Max.X - Min.X + 1,
			Max.Y - Min.Y + 1, Max.Z - Min.Z + 1);
		if (Span < Step) continue;
		for (const FIntVector& Cell : Component)
		{
			const FIntVector Key(
				VoxelGeneration::FloorDivide(Cell.X - Origin.X, Step),
				VoxelGeneration::FloorDivide(Cell.Y - Origin.Y, Step),
				VoxelGeneration::FloorDivide(Cell.Z, Step));
			if (Key.X < 0 || Key.X >= (InOutData.Side - 1) ||
				Key.Y < 0 || Key.Y >= (InOutData.Side - 1)) continue;
			if (!Votes.Contains(Key) && !StructureWorkspace.ReserveMap(Votes, Votes.Num() + 1, OutError)) return false;
			FEditVote& Vote = Votes.FindOrAdd(Key);
			++Vote.Count;
			Vote.Min.X = FMath::Min(Vote.Min.X, Cell.X);
			Vote.Min.Y = FMath::Min(Vote.Min.Y, Cell.Y);
			Vote.Min.Z = FMath::Min(Vote.Min.Z, Cell.Z);
			Vote.Max.X = FMath::Max(Vote.Max.X, Cell.X);
			Vote.Max.Y = FMath::Max(Vote.Max.Y, Cell.Y);
			Vote.Max.Z = FMath::Max(Vote.Max.Z, Cell.Z);
			if (Class == 1) ++Vote.Removed;
			else
			{
				const uint32 PackedState = InModifiedBlocks.FindChecked(Cell).Pack();
				if (!Vote.Placed.Contains(PackedState) &&
					!StructureWorkspace.ReserveMap(Vote.Placed, Vote.Placed.Num() + 1, OutError)) return false;
				++Vote.Placed.FindOrAdd(PackedState);
			}
		}
	}
	TArray<FVoxelDistantCell> PlayerCells;
	TSet<FIntVector> SuppressedCells;
	for (const TPair<FIntVector, FEditVote>& Entry : Votes)
	{
		const FEditVote& Vote = Entry.Value;
		if (Vote.Count < FMath::Max(1, Step / 4)) continue;
		uint32 BestState = 0;
		int32 BestCount = 0;
		for (const TPair<uint32, int32>& Placed : Vote.Placed)
		{
			if (Placed.Value > BestCount ||
				(Placed.Value == BestCount && Placed.Key < BestState))
			{
				BestState = Placed.Key;
				BestCount = Placed.Value;
			}
		}
		if (Vote.Removed >= BestCount)
		{
			if (!SuppressedCells.Contains(Entry.Key) && !StructureWorkspace.ReserveSet(SuppressedCells, SuppressedCells.Num() + 1, OutError)) return false;
			SuppressedCells.Add(Entry.Key);
		}
		else if (BestCount > Vote.Removed)
		{
			const FVoxelBlockState State = FVoxelBlockState::Unpack(BestState);
			const FVoxelRuntimeDefinition* Definition = Registry->Find(State.TypeId);
			if (Definition && Definition->bSolid)
			{
				if ((!SuppressedCells.Contains(Entry.Key) && !StructureWorkspace.ReserveSet(SuppressedCells, SuppressedCells.Num() + 1, OutError)) ||
					!StructureWorkspace.ReserveArray(PlayerCells, PlayerCells.Num() + 1, OutError)) return false;
				SuppressedCells.Add(Entry.Key);
				PlayerCells.Add({Vote.Min, Vote.Max + FIntVector(1), State});
			}
		}
	}
	auto IsSuppressed = [&](const FIntVector& Key)
	{
		return SuppressedCells.Contains(FIntVector(
			VoxelGeneration::FloorDivide(Key.X * StructureStep, Step),
			VoxelGeneration::FloorDivide(Key.Y * StructureStep, Step),
			VoxelGeneration::FloorDivide(Key.Z * StructureStep, Step)));
	};
	TSet<FIntVector> VisibleStructureCells;
	if (StructureStep == Step)
	{
		for (const TPair<FIntVector, FVoxelBlockState>& Entry : StructureCells)
		{
			if (!Entry.Value.IsAir() && !IsSuppressed(Entry.Key))
			{
				if (!StructureWorkspace.ReserveSet(VisibleStructureCells, VisibleStructureCells.Num() + 1, OutError)) return false;
				VisibleStructureCells.Add(Entry.Key);
			}
		}
	}
	else
	{
		FVoxelGenerationWorkspace VisibilityWorkspace(FactResources, CoarseConsumer,
			Generator->GetRequiredInputBytes(), Generator->GetRequiredInputLeaseCount());
		TSet<FIntVector> Visited;
		const FIntVector Neighbors[] = {
			FIntVector(1, 0, 0), FIntVector(-1, 0, 0),
			FIntVector(0, 1, 0), FIntVector(0, -1, 0),
			FIntVector(0, 0, 1), FIntVector(0, 0, -1)
		};
		for (const TPair<FIntVector, FVoxelBlockState>& Entry : StructureCells)
		{
			if (Entry.Value.IsAir() || IsSuppressed(Entry.Key) || Visited.Contains(Entry.Key)) continue;
			FVoxelGenerationWorkspace ComponentWorkspace(FactResources, CoarseConsumer,
				Generator->GetRequiredInputBytes(), Generator->GetRequiredInputLeaseCount());
			TArray<FIntVector> Component;
			if (!ComponentWorkspace.ReserveArray(Component, 1, OutError) ||
				!VisibilityWorkspace.ReserveSet(Visited, Visited.Num() + 1, OutError)) return false;
			Component.Add(Entry.Key);
			Visited.Add(Entry.Key);
			FIntVector Min = Entry.Key;
			FIntVector Max = Entry.Key;
			for (int32 Index = 0; Index < Component.Num(); ++Index)
			{
				const FIntVector Cell = Component[Index];
				Min.X = FMath::Min(Min.X, Cell.X); Min.Y = FMath::Min(Min.Y, Cell.Y); Min.Z = FMath::Min(Min.Z, Cell.Z);
				Max.X = FMath::Max(Max.X, Cell.X); Max.Y = FMath::Max(Max.Y, Cell.Y); Max.Z = FMath::Max(Max.Z, Cell.Z);
				for (const FIntVector& Offset : Neighbors)
				{
					const FIntVector Next = Cell + Offset;
					const FVoxelBlockState* State = StructureCells.Find(Next);
					if (State && !State->IsAir() && !IsSuppressed(Next) && !Visited.Contains(Next))
					{
						if (!VisibilityWorkspace.ReserveSet(Visited, Visited.Num() + 1, OutError) ||
							!ComponentWorkspace.ReserveArray(Component, Component.Num() + 1, OutError)) return false;
						Visited.Add(Next);
						Component.Add(Next);
					}
				}
			}
			const int32 Span = FMath::Max3(Max.X - Min.X + 1,
				Max.Y - Min.Y + 1, Max.Z - Min.Z + 1) * StructureStep;
			if (Span >= Step)
			{
				for (const FIntVector& Cell : Component)
				{
					if (!VisibleStructureCells.Contains(Cell) &&
						!StructureWorkspace.ReserveSet(VisibleStructureCells, VisibleStructureCells.Num() + 1, OutError)) return false;
					VisibleStructureCells.Add(Cell);
				}
			}
		}
	}
	for (const FIntVector& Key : VisibleStructureCells)
	{
		const int32 X = Key.X;
		const int32 Y = Key.Y;
		const int32 GroundX = FMath::Min((InOutData.Side - 1),
			VoxelGeneration::FloorDivide(X * StructureStep, Step));
		const int32 GroundY = FMath::Min((InOutData.Side - 1),
			VoxelGeneration::FloorDivide(Y * StructureStep, Step));
		const int32 Ground = InOutData.GroundZ[GroundX + GroundY * InOutData.Side];
		const int32 Z = Key.Z * StructureStep;
		const int32 Bottom = Ground == MIN_int32 ? Z : FMath::Max(Z, Ground + 1);
		AddCell(FIntVector(Origin.X + X * StructureStep, Origin.Y + Y * StructureStep, Bottom),
			FIntVector(Origin.X + (X + 1) * StructureStep, Origin.Y + (Y + 1) * StructureStep, Z + StructureStep),
			StructureCells.FindChecked(Key));
	}
	for (const FVoxelDistantCell& Cell : PlayerCells)
	{
		AddCell(Cell.Min, Cell.Max, Cell.State);
	}
	// 合并同材质、完整共面的长方体，保留实体范围，避免逐体素输出六个内部面。
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return false; }
		const int32 U = (Axis + 1) % 3;
		const int32 V = (Axis + 2) % 3;
		InOutData.DistantCells.Sort([Axis, U, V](const FVoxelDistantCell& A, const FVoxelDistantCell& B)
		{
			if (A.State.Pack() != B.State.Pack()) return A.State.Pack() < B.State.Pack();
			if (A.SourceTreeId != B.SourceTreeId) return A.SourceTreeId < B.SourceTreeId;
			for (const int32 Component : { U, V })
			{
				if (A.Min[Component] != B.Min[Component]) return A.Min[Component] < B.Min[Component];
				if (A.Max[Component] != B.Max[Component]) return A.Max[Component] < B.Max[Component];
			}
			return A.Min[Axis] != B.Min[Axis] ? A.Min[Axis] < B.Min[Axis] : A.Max[Axis] < B.Max[Axis];
		});
		int32 Count = 0;
		for (int32 Index = 0; Index < InOutData.DistantCells.Num(); ++Index)
		{
			const FVoxelDistantCell Cell = InOutData.DistantCells[Index];
			if (Count > 0)
			{
				FVoxelDistantCell& Previous = InOutData.DistantCells[Count - 1];
				if (Previous.State == Cell.State && Previous.SourceTreeId == Cell.SourceTreeId &&
					Previous.Min[U] == Cell.Min[U] &&
					Previous.Max[U] == Cell.Max[U] && Previous.Min[V] == Cell.Min[V] &&
					Previous.Max[V] == Cell.Max[V] && Previous.Max[Axis] >= Cell.Min[Axis])
				{
					Previous.Max[Axis] = FMath::Max(Previous.Max[Axis], Cell.Max[Axis]);
					continue;
				}
			}
			InOutData.DistantCells[Count++] = Cell;
		}
		InOutData.DistantCells.SetNum(Count, EAllowShrinking::No);
	}
	InOutData.DistantCells.Sort([](const FVoxelDistantCell& A, const FVoxelDistantCell& B)
	{
		if (A.Min.X != B.Min.X) return A.Min.X < B.Min.X;
		if (A.Min.Y != B.Min.Y) return A.Min.Y < B.Min.Y;
		if (A.Min.Z != B.Min.Z) return A.Min.Z < B.Min.Z;
		if (A.Max.X != B.Max.X) return A.Max.X < B.Max.X;
		if (A.Max.Y != B.Max.Y) return A.Max.Y < B.Max.Y;
		if (A.Max.Z != B.Max.Z) return A.Max.Z < B.Max.Z;
		if (A.State.Pack() != B.State.Pack()) return A.State.Pack() < B.State.Pack();
		return A.SourceTreeId < B.SourceTreeId;
	});
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	if (InOutData.DistantCells.Num() > 16384)
	{
		OutError = FString::Printf(TEXT("Distant voxel tile exceeds representation capacity: tile=(%d,%d) level=%d cells=%d"),
			InOutData.Key.Coordinate.X, InOutData.Key.Coordinate.Y, InOutData.Key.Level, InOutData.DistantCells.Num());
		return false;
	}
	OutError.Reset();
	return true;
}

bool FVoxelSurfaceProxyBuilder::ApplyModifiedSurface(
	const FVoxelSurfaceTileKey& InKey,
	FVoxelSurfaceTileData& InOutData,
	const TMap<FIntVector, FVoxelBlockState>& InModifiedBlocks,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_SurfaceApplyModified);

	if (InModifiedBlocks.IsEmpty())
	{
		return true;
	}

	TMap<
		FIntPoint,
		TMap<int32, FVoxelBlockState>>
		ColumnEdits;
	for (const TPair<FIntVector, FVoxelBlockState>& Edit : InModifiedBlocks)
	{
		ColumnEdits.FindOrAdd(FIntPoint(Edit.Key.X, Edit.Key.Y)).Add(
			Edit.Key.Z, Edit.Value);
	}

	for (int32 Y = 0;
		Y < InOutData.Side;
		++Y)
	{
		for (int32 X = 0;
			X < InOutData.Side;
			++X)
		{
			const FIntPoint WorldXY =
				ResolveWorldXY(
					InOutData.GetBuildOrigin(),
					X,
					Y,
					InOutData.Step, InOutData.Side - 1);

			const TMap<
				int32,
				FVoxelBlockState>* Edits =
					ColumnEdits.Find(
						WorldXY);

			if (!Edits ||
				Edits->IsEmpty())
			{
				continue;
			}

			const int32 Index =
				X +
				Y *
					InOutData.Side;

			const int32 NaturalGround = InOutData.GroundZ[Index];
			int32 HighestCandidate = FMath::Max(NaturalGround, InOutData.WaterZ[Index]);
			bool bAffectsSurface = false;
			for (const TPair<int32, FVoxelBlockState>& Edit : *Edits)
			{
				if (Edit.Key < Settings.MinZ || Edit.Key >= Settings.MaxZ)
				{
					continue;
				}
				bAffectsSurface |= Edit.Key >= NaturalGround;
				HighestCandidate = FMath::Max(HighestCandidate, Edit.Key);
			}
			if (!bAffectsSurface)
			{
				continue;
			}

			// 地面和水分别解析；非实体装饰不能成为地表，清空整列不能保留旧高度。
			InOutData.GroundZ[Index] = MIN_int32;
			InOutData.WaterZ[Index] = MIN_int32;
			InOutData.SurfaceStates[Index] = Config->Air;
			HighestCandidate = FMath::Min(HighestCandidate, Settings.MaxZ - 1);
			for (int32 Z = HighestCandidate; Z >= Settings.MinZ; --Z)
			{
				if (InCancel && InCancel->Load())
				{
					OutError = TEXT("Canceled");
					return false;
				}
				FVoxelBlockState State;
				if (const FVoxelBlockState* Modified = Edits->Find(Z))
				{
					State = *Modified;
				}
				else if (!Generator->SampleBlock(FIntVector(WorldXY.X, WorldXY.Y, Z), State, OutError, InCancel))
				{
					return false;
				}
				if (State.IsAir())
				{
					continue;
				}
				const FVoxelRuntimeDefinition* Definition = Registry->Find(State.TypeId);
				if (!Definition)
				{
					OutError = TEXT("Modified voxel surface contains an unknown block type");
					return false;
				}
				if (State.TypeId == Config->Water.TypeId)
				{
					InOutData.WaterZ[Index] = FMath::Max(InOutData.WaterZ[Index], Z);
					continue;
				}
				if (!Definition->bSolid)
				{
					continue;
				}
				InOutData.GroundZ[Index] = Z;
				InOutData.SurfaceStates[Index] = State;
				break;
			}

		}
	}

	return true;
}
