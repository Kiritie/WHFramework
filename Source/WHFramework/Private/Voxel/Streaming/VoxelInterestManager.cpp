#include "Voxel/Streaming/VoxelInterestManager.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelViewLod.h"

namespace
{
	constexpr int32 InterestSectionSide = 16;
	int32 CeilDividePositive(
		const int32 InValue,
		const int32 InDivisor)
	{
		return FMath::Max(
			0,
			(InValue + InDivisor - 1) /
			InDivisor);
	}

	FIntVector InterestToSection(
		const FIntVector& InCell)
	{
		return FIntVector(
			VoxelGeneration::FloorDivide(
				InCell.X,
				InterestSectionSide),
			VoxelGeneration::FloorDivide(
				InCell.Y,
				InterestSectionSide),
			VoxelGeneration::FloorDivide(
				InCell.Z,
				InterestSectionSide));
	}

	bool IsInsideRadius(
		const FIntVector& InDeltaCells,
		const int32 InRadius)
	{
		if (InRadius < 0)
		{
			return false;
		}

		const int64 RadiusSquared =
			static_cast<int64>(InRadius) *
			InRadius;

		return
			static_cast<int64>(InDeltaCells.X) *
				InDeltaCells.X +
			static_cast<int64>(InDeltaCells.Y) *
				InDeltaCells.Y +
			static_cast<int64>(InDeltaCells.Z) *
				InDeltaCells.Z <=
			RadiusSquared;
	}
	bool SectionIntersectsRadius(
		const FIntVector& InSection,
		const FIntVector& InCenter,
		const int32 InRadius)
	{
		if (InRadius <= 0)
		{
			return false;
		}
		const FIntVector Min = InSection * InterestSectionSide;
		const FIntVector Max = Min + FIntVector(InterestSectionSide);
		const FIntVector Nearest(
			FMath::Clamp(InCenter.X, Min.X, Max.X),
			FMath::Clamp(InCenter.Y, Min.Y, Max.Y),
			FMath::Clamp(InCenter.Z, Min.Z, Max.Z));
		return IsInsideRadius(Nearest - InCenter, InRadius);
	}

	bool IsInsideFineVolume(
		const FIntVector& InDeltaCells,
		const int32 InHorizontalRadius,
		const int32 InVerticalRadius)
	{
		if (InHorizontalRadius < 0 ||
			InVerticalRadius < 0 ||
			FMath::Abs(InDeltaCells.Z) > InVerticalRadius)
		{
			return false;
		}

		const int64 RadiusSquared =
			static_cast<int64>(InHorizontalRadius) *
			InHorizontalRadius;

		return
			static_cast<int64>(InDeltaCells.X) *
				InDeltaCells.X +
			static_cast<int64>(InDeltaCells.Y) *
				InDeltaCells.Y <=
			RadiusSquared;
	}

	template<typename KeyType>
	void AddAdaptiveTwoDimensionalTiles(
		const FIntVector& InCenter,
		const int32 InOuterRadius,
		const int32 InInnerRadius,
		const int32 InBaseTileSide,
		const int32 InBaseSampleStepCells,
		const uint8 InMaximumLevel,
		const int32 InMaximumTiles,
		const FVoxelStreamingSource& InSource,
		const float InTargetScreenErrorPixels,
		TSet<KeyType>& OutKeys)
	{
		if (InOuterRadius <= 0 ||
			InBaseTileSide <= 0)
		{
			return;
		}

		struct FNode
		{
			FIntPoint Coordinate;
			uint8 Level = 0;
			double Error = 0.0;
		};

		const int32 RootSide =
			InBaseTileSide << InMaximumLevel;
		const int32 RootX =
			VoxelGeneration::FloorDivide(InCenter.X, RootSide);
		const int32 RootY =
			VoxelGeneration::FloorDivide(InCenter.Y, RootSide);
		const int32 RootRadius =
			CeilDividePositive(
				InOuterRadius,
				RootSide) +
			1;

		TArray<FNode> Leaves;
		auto AddNode = [&](const FIntPoint& Coordinate, const uint8 Level, TArray<FNode>& Nodes)
		{
			const int32 Side = InBaseTileSide << Level;
			const FVector2D Delta(
				Coordinate.X * Side + Side * 0.5 - InCenter.X,
				Coordinate.Y * Side + Side * 0.5 - InCenter.Y);
			const FVector2D NearDelta(
				FMath::Max(0.0, FMath::Abs(Delta.X) - Side * 0.5),
				FMath::Max(0.0, FMath::Abs(Delta.Y) - Side * 0.5));
			const FVector2D FarDelta(FMath::Abs(Delta.X) + Side * 0.5, FMath::Abs(Delta.Y) + Side * 0.5);
			if (NearDelta.SizeSquared() > FMath::Square(static_cast<double>(InOuterRadius)) ||
				(InInnerRadius > 0 && FarDelta.SizeSquared() <= FMath::Square(static_cast<double>(InInnerRadius))))
			{
				return;
			}
			const double Distance = FMath::Max(1.0, NearDelta.Size());
			const uint8 DesiredLevel = VoxelViewLod::ResolveScreenErrorLevel(
				FMath::FloorToInt(Distance), InBaseSampleStepCells, InSource, InMaximumLevel, InTargetScreenErrorPixels);
			const double Error = Level > DesiredLevel ? static_cast<double>(InBaseSampleStepCells << Level) / Distance : 0.0;
			Nodes.Add({ Coordinate, Level, Error });
		};

		for (int32 Y = -RootRadius;
			Y <= RootRadius;
			++Y)
		{
			for (int32 X = -RootRadius;
				X <= RootRadius;
				++X)
			{
				AddNode(FIntPoint(RootX + X, RootY + Y), InMaximumLevel, Leaves);
			}
		}

		while (!Leaves.IsEmpty())
		{
			double MaximumError = 0.0;
			for (const FNode& Node : Leaves)
			{
				MaximumError = FMath::Max(MaximumError, Node.Error);
			}
			if (MaximumError <= 0.0)
			{
				break;
			}
			TArray<FNode> Children;
			int32 ParentCount = 0;
			for (const FNode& Node : Leaves)
			{
				if (!FMath::IsNearlyEqual(Node.Error, MaximumError, UE_DOUBLE_SMALL_NUMBER))
				{
					continue;
				}
				++ParentCount;
				const FIntPoint ChildBase = Node.Coordinate * 2;
				for (int32 Y = 0; Y < 2; ++Y)
				{
					for (int32 X = 0; X < 2; ++X)
					{
						AddNode(ChildBase + FIntPoint(X, Y), Node.Level - 1, Children);
					}
				}
			}
			if (Leaves.Num() - ParentCount + Children.Num() > InMaximumTiles)
			{
				break;
			}
			Leaves.RemoveAll([MaximumError](const FNode& Node)
			{
				return FMath::IsNearlyEqual(Node.Error, MaximumError, UE_DOUBLE_SMALL_NUMBER);
			});
			Leaves.Append(MoveTemp(Children));
		}
		for (const FNode& Node : Leaves)
		{
			OutKeys.Add({ Node.Coordinate, Node.Level });
		}
	}

	template<typename KeyType>
	void BalanceAdaptiveTiles(TSet<KeyType>& InOutTiles,
		const int32 InBaseTileSide, const uint8 InMaximumLevel)
	{
		while (true)
		{
			TSet<KeyType> ParentsToSplit;
			for (const KeyType& Tile : InOutTiles)
			{
				const int32 Side = InBaseTileSide << Tile.Level;
				const FIntPoint Min = Tile.Coordinate * Side;
				const FIntPoint Max = Min + FIntPoint(Side);
				const FIntPoint Probes[] = {
					FIntPoint(Min.X - 1, Min.Y), FIntPoint(Min.X - 1, Max.Y - 1),
					FIntPoint(Max.X, Min.Y), FIntPoint(Max.X, Max.Y - 1),
					FIntPoint(Min.X, Min.Y - 1), FIntPoint(Max.X - 1, Min.Y - 1),
					FIntPoint(Min.X, Max.Y), FIntPoint(Max.X - 1, Max.Y)
				};
				for (uint8 Level = Tile.Level + 1; Level <= InMaximumLevel; ++Level)
				{
					const int32 CandidateSide = InBaseTileSide << Level;
					const KeyType Ancestor{
						FIntPoint(VoxelGeneration::FloorDivide(Min.X, CandidateSide),
							VoxelGeneration::FloorDivide(Min.Y, CandidateSide)), Level};
					if (InOutTiles.Contains(Ancestor)) ParentsToSplit.Add(Ancestor);
					if (Level <= Tile.Level + 1) continue;
					for (const FIntPoint& Probe : Probes)
					{
						const KeyType Neighbor{
							FIntPoint(VoxelGeneration::FloorDivide(Probe.X, CandidateSide),
								VoxelGeneration::FloorDivide(Probe.Y, CandidateSide)), Level};
						if (InOutTiles.Contains(Neighbor)) ParentsToSplit.Add(Neighbor);
					}
				}
			}
			if (ParentsToSplit.IsEmpty()) break;
			for (const KeyType& Parent : ParentsToSplit)
			{
				if (!InOutTiles.Remove(Parent)) continue;
				const FIntPoint ChildBase = Parent.Coordinate * 2;
				for (int32 Y = 0; Y < 2; ++Y)
				{
					for (int32 X = 0; X < 2; ++X)
					{
						InOutTiles.Add({ChildBase + FIntPoint(X, Y), static_cast<uint8>(Parent.Level - 1)});
					}
				}
			}
		}
	}
}

FVoxelInterestSet FVoxelInterestManager::Compute(
	TConstArrayView<FVoxelStreamingSource> InSources,
	const FVoxelWorldManifest& InManifest,
	const FVoxelViewSettings& InViewSettings,
	const FVoxelInterestSet* InPrevious) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ViewInterestPlan);

	FVoxelInterestSet Prepared;

	for (const FVoxelStreamingSource& Source :
		InSources)
	{
		AddExactSource(
			Source,
			InManifest,
			InViewSettings,
			Prepared,
			InPrevious);

		AddViewSource(
			Source,
			InManifest,
			InViewSettings,
			Prepared, true, true, true);
	}
	return Finalize(InSources, InManifest, InViewSettings, MoveTemp(Prepared));
}

void FVoxelInterestManager::BuildSourceExact(const FVoxelStreamingSource& InSource,
	const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
	FVoxelInterestSet& OutInterest, const FVoxelInterestSet* InPrevious) const
{
	AddExactSource(InSource, InManifest, InSettings, OutInterest, InPrevious);
}

void FVoxelInterestManager::BuildSourceProxy(const FVoxelStreamingSource& InSource,
	const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
	FVoxelInterestSet& OutInterest) const
{
	AddViewSource(InSource, InManifest, InSettings, OutInterest, true, false, false);
}

void FVoxelInterestManager::BuildSourceSurface(const FVoxelStreamingSource& InSource,
	const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
	FVoxelInterestSet& OutInterest) const
{
	AddViewSource(InSource, InManifest, InSettings, OutInterest, false, true, false);
}

void FVoxelInterestManager::BuildSourceMacro(const FVoxelStreamingSource& InSource,
	const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
	FVoxelInterestSet& OutInterest) const
{
	AddViewSource(InSource, InManifest, InSettings, OutInterest, false, false, true);
}

FVoxelInterestSet FVoxelInterestManager::Finalize(TConstArrayView<FVoxelStreamingSource> InSources,
	const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InViewSettings,
	FVoxelInterestSet Result) const
{
	uint8 MaximumSurfaceLevel = 0;
	uint8 MaximumMacroLevel = 0;
	for (const FVoxelStreamingSource& Source : InSources)
	{
		if (Source.Has(EVoxelStreamingCapability::WorldVisual))
		{
			MaximumSurfaceLevel = FMath::Max(MaximumSurfaceLevel, Source.View.MaximumSurfaceLevel);
			MaximumMacroLevel = FMath::Max(MaximumMacroLevel, InViewSettings.MaximumMacroLevel);
		}
	}
	BalanceAdaptiveTiles(Result.Surface, InViewSettings.SurfaceTileSide,
		MaximumSurfaceLevel);
	BalanceAdaptiveTiles(Result.Macro, InViewSettings.MacroTileSide,
		MaximumMacroLevel);

	if (!Result.VoxelProxy.IsEmpty())
	{
		TSet<FIntVector> FineSections;
		for (const TPair<FIntVector, FVoxelExactDemand>& Pair : Result.Exact)
		{
			if (Pair.Value.bFineRender)
			{
				FineSections.Add(Pair.Key);
			}
		}
		Result.TerrainPlan.Build(InSources, FineSections, Result.VoxelProxy, InViewSettings, InViewSettings.MaximumTerrainLeaves);
		Result.VoxelProxy.Reset();
		for (const FVoxelViewKey& Key : Result.TerrainPlan.Leaves)
		{
			if (Key.Level > 0)
			{
				Result.VoxelProxy.Add(Key);
				continue;
			}
			FVoxelExactDemand& Demand = Result.Exact.FindOrAdd(Key.Coordinate);
			Demand.bData = true;
			Demand.bFineRender = true;
			Demand.bFineData = true;
			const FVector Center = FVector(Key.Coordinate * InterestSectionSide) + FVector(InterestSectionSide * 0.5);
			int32 RenderPriority = MAX_int32;
			for (const FVoxelStreamingSource& Source : InSources)
			{
				if (Source.Has(EVoxelStreamingCapability::FineVisual))
				{
					Demand.DistanceCells = FMath::Min(Demand.DistanceCells, FVector::Distance(Center, FVector(Source.Center)));
					Demand.HorizontalDistanceCells = FMath::Min(Demand.HorizontalDistanceCells,
						FVector2D::Distance(FVector2D(Center), FVector2D(Source.Center.X, Source.Center.Y)));
				}
			}
			for (const FVoxelStreamingSource& Source : InSources)
			{
				const int32 Radius = Source.View.FineRadiusCells;
				if (Source.Has(EVoxelStreamingCapability::FineVisual) &&
					FVector2D::DistSquared(FVector2D(Center), FVector2D(Source.Center.X, Source.Center.Y)) <=
					FMath::Square(static_cast<double>(Radius + Source.View.FinePreloadCells)))
				{
					RenderPriority = FMath::Min(RenderPriority, Source.GetSchedulingPriority());
				}
			}
			if (RenderPriority == MAX_int32)
			{
				double NearestDistanceSquared = MAX_dbl;
				for (const FVoxelStreamingSource& Source : InSources)
				{
					if (!Source.Has(EVoxelStreamingCapability::FineVisual)) continue;
					const double DistanceSquared = FVector::DistSquared(Center, FVector(Source.Center));
					if (DistanceSquared < NearestDistanceSquared)
					{
						NearestDistanceSquared = DistanceSquared;
						RenderPriority = Source.GetSchedulingPriority();
					}
				}
			}
			Demand.Priority = FMath::Min(Demand.Priority, RenderPriority);
		}
	}
	// 精细边界需要真实相邻数据；只增加数据依赖，不扩张碰撞、模拟或可见几何。
	TArray<FIntVector> FineKeys;
	for (const auto& Pair : Result.Exact)
	{
		if (Pair.Value.bFineRender)
		{
			FineKeys.Add(Pair.Key);
		}
	}
	const auto FineSnapshot = MakeShared<TSet<FIntVector>, ESPMode::ThreadSafe>();
	FineSnapshot->Append(FineKeys);
	Result.FineSections = FineSnapshot;
	for (const FIntVector& Key : FineKeys)
	{
		const FVector2D Center(Key.X * InterestSectionSide + InterestSectionSide * 0.5,
			Key.Y * InterestSectionSide + InterestSectionSide * 0.5);
		for (const FVoxelStreamingSource& Source : InSources)
		{
			const double PlayableRadius = Source.View.FineRadiusCells * InViewSettings.PlayableFineRadiusFraction;
			if (Source.Has(EVoxelStreamingCapability::Readiness) && Source.Has(EVoxelStreamingCapability::FineVisual) &&
				FVector2D::DistSquared(Center, FVector2D(Source.Center.X, Source.Center.Y)) <= FMath::Square(PlayableRadius))
			{
				Result.PlayableFineKeys.Add(Key);
				break;
			}
		}
	}
	TArray<FIntVector> FineDataKeys;
	for (const auto& Pair : Result.Exact)
	{
		if (Pair.Value.bFineData) FineDataKeys.Add(Pair.Key);
	}
	for (const FIntVector& Key : FineDataKeys)
	{
		const double Distance = Result.Exact.FindChecked(Key).DistanceCells;
		const int32 Priority = Result.Exact.FindChecked(Key).Priority;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			for (const int32 Sign : { -1, 1 })
			{
				FIntVector Neighbor = Key;
				Neighbor[Axis] += Sign;
				if (Neighbor.Z * InterestSectionSide >= InManifest.Settings.MaxZ ||
					(Neighbor.Z + 1) * InterestSectionSide <= InManifest.Settings.MinZ)
				{
					continue;
				}
				FVoxelExactDemand& Demand = Result.Exact.FindOrAdd(Neighbor);
				Demand.bData = true;
				Demand.DistanceCells = FMath::Min(Demand.DistanceCells, Distance + InterestSectionSide);
				Demand.Priority = FMath::Min(Demand.Priority, Priority);
			}
		}
	}

	// 当前位置与预测位置之间都属于关键范围，前瞻距离不能把当前位置排除在外。
	TSet<FIntVector> CriticalFine;
	for (const FVoxelStreamingSource& Source : InSources)
	{
		if (!Source.Has(EVoxelStreamingCapability::FineVisual) ||
			Source.Has(EVoxelStreamingCapability::LocalRefinement) ||
			Source.View.MovementCriticalFineRadiusCells <= 0) continue;
		const FVector Current(Source.Center);
		const FVector Predicted = Current + Source.VelocityCellsPerSecond * Source.View.FinePredictionSeconds;
		const double RadiusSquared = FMath::Square(static_cast<double>(Source.View.MovementCriticalFineRadiusCells));
		for (auto& Pair : Result.Exact)
		{
			if (!Pair.Value.bFineRender) continue;
			const FVector Center(Pair.Key * InterestSectionSide + FIntVector(InterestSectionSide / 2));
			if (FMath::PointDistToSegmentSquared(Center, Current, Predicted) > RadiusSquared) continue;
			Pair.Value.bMovementCriticalFine = true;
			Pair.Value.bMovementCriticalData = true;
			CriticalFine.Add(Pair.Key);
		}
	}
	for (const FIntVector& Key : CriticalFine)
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			for (const int32 Sign : {-1, 1})
			{
				FIntVector Neighbor = Key;
				Neighbor[Axis] += Sign;
				if (FVoxelExactDemand* Demand = Result.Exact.Find(Neighbor)) Demand->bMovementCriticalData = true;
			}
		}
	}
	// 排序随不可变范围快照在后台完成，主线程只消费索引，不重新分配、排序数万个节点。
	auto Distance = [&InSources](const FBox& Bounds, const bool bColumn, const bool bFullOnly, const int32 InPriority)
	{
		double Best = MAX_dbl;
		for (const FVoxelStreamingSource& Source : InSources)
		{
			if (!Source.Has(EVoxelStreamingCapability::FineVisual) || Source.GetSchedulingPriority() != InPriority ||
				(bFullOnly && !Source.Has(EVoxelStreamingCapability::WorldVisual))) continue;
			FVector Position(Source.Center);
			if (bColumn) Position.Z = 0.0;
			Best = FMath::Min(Best, FMath::Sqrt(Bounds.ComputeSquaredDistanceToPoint(Position)));
		}
		return Best;
	};
	auto ViewPriority = [&InSources](const FBox& Bounds, const bool bColumn,
		const int32 FVoxelStreamingSourceView::* RadiusField, const int32 WorldRadius = 0)
	{
		int32 Priority = MAX_int32;
		int32 RefinementPriority = MAX_int32;
		int32 NearestPriority = MAX_int32;
		double NearestDistanceSquared = MAX_dbl;
		for (const FVoxelStreamingSource& Source : InSources)
		{
			if (!Source.Has(EVoxelStreamingCapability::FineVisual) ||
				(!RadiusField && !Source.Has(EVoxelStreamingCapability::WorldVisual)))
			{
				continue;
			}
			FVector Position(Source.Center);
			if (bColumn) Position.Z = 0.0;
			const double DistanceSquared = Bounds.ComputeSquaredDistanceToPoint(Position);
			if (RadiusField == &FVoxelStreamingSourceView::VoxelProxyRadiusCells &&
				DistanceSquared <= FMath::Square(static_cast<double>(Source.View.FineRadiusCells + Source.View.FinePreloadCells + InterestSectionSide)))
				RefinementPriority = FMath::Min(RefinementPriority, Source.GetSchedulingPriority());
			if (DistanceSquared < NearestDistanceSquared)
			{
				NearestDistanceSquared = DistanceSquared;
				NearestPriority = Source.GetSchedulingPriority();
			}
			if (!Source.Has(EVoxelStreamingCapability::WorldVisual) &&
				!Source.Has(EVoxelStreamingCapability::LocalRefinement)) continue;
			const int32 Radius = RadiusField ? Source.View.*RadiusField : WorldRadius;
			if (Radius > 0 && DistanceSquared <= FMath::Square(static_cast<double>(Radius)))
			{
				Priority = FMath::Min(Priority, Source.GetSchedulingPriority());
			}
		}
		return RefinementPriority != MAX_int32 ? RefinementPriority : Priority != MAX_int32 ? Priority : NearestPriority;
	};
	for (const auto& Pair : Result.Exact)
	{
		if (!Pair.Value.bFineRender) continue;
		FVoxelViewAdmission& A = Result.Admissions.AddDefaulted_GetRef();
		A.Kind = EVoxelViewAdmissionKind::Fine;
		A.FineKey = Pair.Key;
		const FVector Center(Pair.Key * InterestSectionSide + FIntVector(InterestSectionSide / 2));
		A.Priority = Pair.Value.Priority;
		A.DistanceCells = Distance(FBox(Center, Center), false, false, A.Priority);
	}
	for (const FVoxelViewKey& Key : Result.VoxelProxy)
	{
		FVoxelViewAdmission& A = Result.Admissions.AddDefaulted_GetRef();
		A.Kind = EVoxelViewAdmissionKind::VoxelProxy;
		A.ProxyKey = Key;
		const FVoxelGenerationBounds Bounds = Key.GetBounds();
		const FBox Box(FVector(Bounds.Min), FVector(Bounds.Max));
		A.Priority = ViewPriority(Box, false, &FVoxelStreamingSourceView::VoxelProxyRadiusCells);
		A.DistanceCells = Distance(Box, false, false, A.Priority);
		Result.VoxelProxyPriorities.Add(Key, A.Priority);
	}
	for (const FVoxelSurfaceTileKey& Key : Result.Surface)
	{
		FVoxelViewAdmission& A = Result.Admissions.AddDefaulted_GetRef();
		A.Kind = EVoxelViewAdmissionKind::Surface;
		A.SurfaceKey = Key;
		const int32 Side = InViewSettings.SurfaceTileSide << Key.Level;
		const FVector Center((Key.Coordinate.X + 0.5) * Side, (Key.Coordinate.Y + 0.5) * Side, 0.0);
		const FVector Min(Key.Coordinate.X * Side, Key.Coordinate.Y * Side, 0.0);
		A.Priority = ViewPriority(FBox(Min, Min + FVector(Side, Side, 0.0)), true,
			&FVoxelStreamingSourceView::SurfaceRadiusCells);
		A.DistanceCells = Distance(FBox(Center, Center), true, true, A.Priority);
		Result.SurfacePriorities.Add(Key, A.Priority);
	}
	for (const FVoxelMacroTileKey& Key : Result.Macro)
	{
		FVoxelViewAdmission& A = Result.Admissions.AddDefaulted_GetRef();
		A.Kind = EVoxelViewAdmissionKind::Macro;
		A.MacroKey = Key;
		const int32 Side = InViewSettings.MacroTileSide << Key.Level;
		const FVector Center((Key.Coordinate.X + 0.5) * Side, (Key.Coordinate.Y + 0.5) * Side, 0.0);
		const FVector Min(Key.Coordinate.X * Side, Key.Coordinate.Y * Side, 0.0);
		A.Priority = ViewPriority(FBox(Min, Min + FVector(Side, Side, 0.0)), true,
			nullptr, InViewSettings.MacroRadiusCells);
		A.DistanceCells = Distance(FBox(Center, Center), true, true, A.Priority);
		Result.MacroPriorities.Add(Key, A.Priority);
	}
	Result.Admissions.Sort();
	for (int32 Index = 0; Index < Result.Admissions.Num(); ++Index)
	{
		Result.AdmissionLanes[static_cast<uint8>(Result.Admissions[Index].Kind)].Add(Index);
	}

	for (auto& Pair : Result.Sources)
	{
		FVoxelGenerationBounds& Bounds = Pair.Value.FineBounds;
		Bounds = {};
		for (const FIntVector& Section : Pair.Value.FineDataSections)
		{
			const FIntVector Min = Section * InterestSectionSide;
			const FIntVector Max = Min + FIntVector(InterestSectionSide);
			if (!Bounds.IsValid()) Bounds = {Min, Max};
			else
			{
				Bounds.Min = Bounds.Min.ComponentMin(Min);
				Bounds.Max = Bounds.Max.ComponentMax(Max);
			}
		}
	}
	SortExactDemands(Result);
	return Result;
}

void FVoxelInterestManager::SortExactDemands(FVoxelInterestSet& Result)
{
	Result.Exact.GetKeys(Result.ExactOrder);
	Result.ExactOrder.Sort([&Result](const FIntVector& Left, const FIntVector& Right)
	{
		const FVoxelExactDemand& A = Result.Exact.FindChecked(Left);
		const FVoxelExactDemand& B = Result.Exact.FindChecked(Right);
		const bool bGameplayA = A.bExact || A.bCollision || A.bSimulation || A.bWarmupData;
		const bool bGameplayB = B.bExact || B.bCollision || B.bSimulation || B.bWarmupData;
		if (A.Priority != B.Priority) return A.Priority < B.Priority;
		const bool bCriticalA = A.bMovementCriticalData || A.bMovementCriticalFine || A.bMovementCriticalCollision || A.bWarmupCollision;
		const bool bCriticalB = B.bMovementCriticalData || B.bMovementCriticalFine || B.bMovementCriticalCollision || B.bWarmupCollision;
		if (bCriticalA != bCriticalB) return bCriticalA;
		if (bGameplayA != bGameplayB) return bGameplayA;
		if (A.DistanceCells != B.DistanceCells) return A.DistanceCells < B.DistanceCells;
		if (A.ForwardScore != B.ForwardScore) return A.ForwardScore > B.ForwardScore;
		if (Left.X != Right.X) return Left.X < Right.X;
		if (Left.Y != Right.Y) return Left.Y < Right.Y;
		return Left.Z < Right.Z;
	});
	for (auto& Lane : Result.ExactAdmissionLanes) Lane.Reset();
	for (int32 Index = 0; Index < Result.ExactOrder.Num(); ++Index)
	{
		const FVoxelExactDemand& Demand = Result.Exact.FindChecked(Result.ExactOrder[Index]);
		const bool bCritical = Demand.bMovementCriticalData || Demand.bMovementCriticalFine ||
			Demand.bMovementCriticalCollision || Demand.bWarmupCollision;
		Result.ExactAdmissionLanes[bCritical ? 0 : 1].Add(Index);
	}
}

void FVoxelInterestManager::AddExactSource(
	const FVoxelStreamingSource& InSource,
	const FVoxelWorldManifest& InManifest,
	const FVoxelViewSettings& InViewSettings,
	FVoxelInterestSet& InOutInterest,
	const FVoxelInterestSet* InPrevious) const
{
	FVoxelSourceInterest& SourceInterest = InOutInterest.Sources.FindOrAdd(InSource.Id);
	SourceInterest.Source = InSource;
	if (!InSource.Has(EVoxelStreamingCapability::Data))
	{
		return;
	}
	const bool bPrewarm = InSource.Purpose == EVoxelStreamingSourcePurpose::TravelPrewarm ||
		InSource.Purpose == EVoxelStreamingSourcePurpose::RespawnPrewarm;
	const int32 ExactRadius = InSource.ExactRadius;

	const int32 CollisionRadius =
		InSource.Has(EVoxelStreamingCapability::Collision)
			? FMath::Max(
				0,
				InSource.CollisionRadius)
			: 0;

	const int32 SimulationRadius =
		InSource.Has(EVoxelStreamingCapability::Simulation)
			? FMath::Max(
				0,
				InSource.SimulationRadius)
			: 0;

	const bool bWantsFine =
		InSource.Has(EVoxelStreamingCapability::FineVisual) &&
		InSource.View.FineRadiusCells > 0 &&
		InSource.View.FineVerticalRadiusCells > 0;

		const int32 FineRadius = bWantsFine ? FMath::Max(0, InSource.View.FineRadiusCells) +
		FMath::Max(0, InSource.View.FinePreloadCells) : 0;
	const int32 FineVerticalRadius =
		bWantsFine
			? FMath::Max(0, InSource.View.FineVerticalRadiusCells)
			: 0;

	const int32 WarmupDataRadius =
		FMath::Min(
			bPrewarm ? ExactRadius : FMath::Max(0, InSource.View.WarmupDataRadiusCells),
			ExactRadius);

	const int32 WarmupCollisionRadius =
		InSource.Has(EVoxelStreamingCapability::Collision)
			? FMath::Min(
				bPrewarm ? CollisionRadius : FMath::Max(0, InSource.View.WarmupCollisionRadiusCells),
				CollisionRadius)
			: 0;

	const int32 MovementCriticalCollisionRadius =
		InSource.Has(EVoxelStreamingCapability::Collision)
			? FMath::Min(
				CollisionRadius,
				FMath::Max(0, InSource.MovementCriticalCollisionRadius))
			: 0;

	const int32 FineRetainRadius = bWantsFine && FineRadius > 0 ? FineRadius + InterestSectionSide : 0;
	const int32 FineRetainVerticalRadius = bWantsFine && FineVerticalRadius > 0
		? FineVerticalRadius + InterestSectionSide
		: 0;
	const int32 DataRadius = FMath::Max(FineRetainRadius,
		FMath::Max(
			FMath::Max(
				ExactRadius,
				CollisionRadius),
			SimulationRadius));

	const int32 HorizontalSections =
		CeilDividePositive(
			DataRadius,
			InterestSectionSide);

	const int32 VerticalSections = CeilDividePositive(
		FMath::Max(FineRetainVerticalRadius, FMath::Max(0, InSource.VerticalExactRadius)), InterestSectionSide);

	const FIntVector CenterSection =
		InterestToSection(
			InSource.Center);

	const FVector Direction =
		InSource.Direction.GetSafeNormal();

	const int32 MinSectionZ =
		VoxelGeneration::FloorDivide(
			InManifest.Settings.MinZ,
			InterestSectionSide);

	const int32 MaxSectionZ =
		VoxelGeneration::FloorDivide(
			InManifest.Settings.MaxZ - 1,
			InterestSectionSide);

	for (int32 Z = -VerticalSections;
		Z <= VerticalSections;
		++Z)
	{
		for (int32 Y = -HorizontalSections;
			Y <= HorizontalSections;
			++Y)
		{
			for (int32 X = -HorizontalSections;
				X <= HorizontalSections;
				++X)
			{
				const FIntVector Key =
					CenterSection +
					FIntVector(X, Y, Z);

				if (Key.Z < MinSectionZ ||
					Key.Z > MaxSectionZ)
				{
					continue;
				}

				const FIntVector SectionCenterCells =
					Key * InterestSectionSide +
					FIntVector(
						InterestSectionSide / 2);

				const FIntVector Delta =
					SectionCenterCells -
					InSource.Center;

				const bool bExact =
					IsInsideRadius(
						Delta,
						ExactRadius);

				const bool bCollision =
					InSource.Has(EVoxelStreamingCapability::Collision) &&
					IsInsideRadius(
						Delta,
						CollisionRadius);

				const bool bSimulation =
					InSource.Has(EVoxelStreamingCapability::Simulation) &&
					SimulationRadius > 0 &&
					SectionIntersectsRadius(Key, InSource.Center, SimulationRadius);

				const FVoxelExactDemand* Previous = InPrevious ? InPrevious->Exact.Find(Key) : nullptr;
				const bool bRetainFine = InSource.Has(EVoxelStreamingCapability::FineVisual) &&
					!InSource.Has(EVoxelStreamingCapability::LocalRefinement) && (InPrevious && InPrevious->FineSections
					? InPrevious->FineSections->Contains(Key) : Previous && Previous->bFineRender);
				const bool bFineRender =
					bWantsFine &&
					IsInsideFineVolume(
						FIntVector(FMath::Max(0, FMath::Abs(Delta.X) - InterestSectionSide / 2),
							FMath::Max(0, FMath::Abs(Delta.Y) - InterestSectionSide / 2),
							FMath::Max(0, FMath::Abs(Delta.Z) - InterestSectionSide / 2)),
						bRetainFine ? FineRetainRadius : FineRadius,
						bRetainFine ? FineRetainVerticalRadius : FineVerticalRadius);

				const bool bWarmupData =
					SectionIntersectsRadius(Key, InSource.Center, WarmupDataRadius);

				const bool bWarmupCollision =
					InSource.Has(EVoxelStreamingCapability::Collision) &&
					SectionIntersectsRadius(Key, InSource.Center, WarmupCollisionRadius);

				const bool bMovementCriticalCollision =
					InSource.Has(EVoxelStreamingCapability::Collision) &&
					MovementCriticalCollisionRadius > 0 &&
					IsInsideRadius(Delta, MovementCriticalCollisionRadius);

				if (!bExact &&
					!bCollision &&
					!bSimulation &&
					!bFineRender &&
					!bWarmupData &&
					!bWarmupCollision &&
					!bMovementCriticalCollision)
				{
					continue;
				}

				FVoxelExactDemand& Demand =
					InOutInterest.Exact.
						FindOrAdd(Key);

				if (bExact || bCollision || bSimulation || bWarmupData || bWarmupCollision || bMovementCriticalCollision)
				{
					SourceInterest.DataSections.Add(Key);
				}
				if (bCollision || bWarmupCollision || bMovementCriticalCollision)
				{
					SourceInterest.CollisionSections.Add(Key);
				}
				if (InSource.Has(EVoxelStreamingCapability::Readiness) && bWarmupData)
				{
					FVoxelExactDemand& Global = InOutInterest.Warmup.FindOrAdd(Key);
					Global.bWarmupData = true;
					Global.bWarmupCollision |= bWarmupCollision;
					Global.bFineRender |= bFineRender;
				}
				if (bFineRender)
				{
					SourceInterest.FineDataSections.Add(Key);
					for (int32 Axis = 0; Axis < 3; ++Axis)
					{
						for (const int32 Sign : { -1, 1 })
						{
							FIntVector Neighbor = Key;
							Neighbor[Axis] += Sign;
							if (Neighbor.Z * InterestSectionSide < InManifest.Settings.MaxZ &&
								(Neighbor.Z + 1) * InterestSectionSide > InManifest.Settings.MinZ)
								SourceInterest.FineDataSections.Add(Neighbor);
						}
					}
				}
				Demand.bData = true;
				Demand.bExact |= bExact;
				Demand.bCollision |= bCollision;
				Demand.bSimulation |= bSimulation;
				Demand.bFineRender |= bFineRender;
				Demand.bFineData |= bFineRender;
				Demand.bWarmupData |= bWarmupData;
				Demand.bWarmupCollision |= bWarmupCollision;
				Demand.bMovementCriticalCollision |= bMovementCriticalCollision;
				Demand.Priority = FMath::Min(Demand.Priority, InSource.GetSchedulingPriority());

				const FVector DeltaVector(
					Delta);

				const double Distance =
					DeltaVector.Size();

				const double Forward =
					Distance >
					UE_DOUBLE_SMALL_NUMBER
						? FVector::DotProduct(
							Direction,
							DeltaVector /
								Distance)
						: 1.0;

				Demand.DistanceCells = FMath::Min(Demand.DistanceCells, Distance);
				Demand.HorizontalDistanceCells = FMath::Min(Demand.HorizontalDistanceCells,
					FVector2D(Delta.X, Delta.Y).Size());
				Demand.ForwardScore = FMath::Max(Demand.ForwardScore, Forward);
			}
		}
	}
}

void FVoxelInterestManager::AddViewSource(
	const FVoxelStreamingSource& InSource,
	const FVoxelWorldManifest& InManifest,
	const FVoxelViewSettings& InViewSettings,
	FVoxelInterestSet& InOutInterest,
	const bool bBuildProxy,
	const bool bBuildSurface,
	const bool bBuildMacro) const
{
	if (!InSource.Has(EVoxelStreamingCapability::WorldVisual) &&
		!InSource.Has(EVoxelStreamingCapability::LocalRefinement))
	{
		return;
	}

	const int32 FineRadius = FMath::Max(0, InSource.View.FineRadiusCells) + FMath::Max(0, InSource.View.FinePreloadCells);
	const int32 FineVerticalRadius = FMath::Max(0, InSource.View.FineVerticalRadiusCells);

	const bool bLocalRefinementOnly =
		InSource.Has(EVoxelStreamingCapability::LocalRefinement) &&
		!InSource.Has(EVoxelStreamingCapability::WorldVisual);
	const int32 ProxyRange = bLocalRefinementOnly
		? static_cast<int32>(FMath::Min<int64>(
			static_cast<int64>(FineRadius) +
			FMath::Max(InterestSectionSide, InViewSettings.VoxelProxyTileSide * 2),
			MAX_int32))
		: FMath::Max(FineRadius, InSource.View.VoxelProxyRadiusCells);

	if (bBuildProxy && ProxyRange > FineRadius)
	{
		const uint8 ProxyLevel = bLocalRefinementOnly ? 1 :
			FMath::Max<uint8>(1, InSource.View.MaximumVoxelProxyLevel);

		const int32 ProxySide =
			FMath::Max(
				InterestSectionSide,
				InViewSettings.
					VoxelProxyTileSide <<
				ProxyLevel);

		const int32 HorizontalRadius =
			CeilDividePositive(
				ProxyRange,
				ProxySide) +
			1;

		const int32 VerticalRadius =
			FMath::Max(
				1,
				CeilDividePositive(
					FMath::Max(
						FineVerticalRadius,
						FMath::Max(
							0,
							InSource.VerticalExactRadius)),
					ProxySide));

		const FIntVector ProxyCenter(
			VoxelGeneration::FloorDivide(
				InSource.Center.X,
				ProxySide),
			VoxelGeneration::FloorDivide(
				InSource.Center.Y,
				ProxySide),
			VoxelGeneration::FloorDivide(
				InSource.Center.Z,
				ProxySide));

		const int32 MinZ =
			VoxelGeneration::FloorDivide(
				InManifest.Settings.MinZ,
				ProxySide);

		const int32 MaxZ =
			VoxelGeneration::FloorDivide(
				InManifest.Settings.MaxZ - 1,
				ProxySide);

		const double HalfDiagonal =
			static_cast<double>(ProxySide) *
			0.8660254037844386;

		for (int32 Z = -VerticalRadius;
			Z <= VerticalRadius;
			++Z)
		{
			for (int32 Y = -HorizontalRadius;
				Y <= HorizontalRadius;
				++Y)
			{
				for (int32 X = -HorizontalRadius;
					X <= HorizontalRadius;
					++X)
				{
					const FIntVector Coordinate =
						ProxyCenter +
						FIntVector(X, Y, Z);

					if (Coordinate.Z < MinZ ||
						Coordinate.Z > MaxZ)
					{
						continue;
					}

					const FVector TileCenter =
						FVector(
							Coordinate *
								ProxySide) +
						FVector(
							ProxySide * 0.5);

					const double Distance =
						FVector::Distance(
							TileCenter,
							FVector(
								InSource.Center));

					if (Distance -
						HalfDiagonal >
						ProxyRange)
					{
						continue;
					}

					if (FineRadius > 0 && FineVerticalRadius > 0)
					{
						const FVector TileMin = FVector(Coordinate * ProxySide);
						const FVector TileMax = TileMin + FVector(ProxySide);
						const double FarX = FMath::Max(
							FMath::Abs(TileMin.X - InSource.Center.X),
							FMath::Abs(TileMax.X - InSource.Center.X));
						const double FarY = FMath::Max(
							FMath::Abs(TileMin.Y - InSource.Center.Y),
							FMath::Abs(TileMax.Y - InSource.Center.Y));
						const bool bInsideFineHorizontal =
							FarX * FarX + FarY * FarY <= FMath::Square(static_cast<double>(FineRadius));
						const bool bInsideFineVertical =
							TileMin.Z >= InSource.Center.Z - FineVerticalRadius &&
							TileMax.Z <= InSource.Center.Z + FineVerticalRadius;
						if (bInsideFineHorizontal && bInsideFineVertical)
						{
							continue;
						}
					}

					InOutInterest.
						VoxelProxy.Add({
							Coordinate,
							ProxyLevel
						});
				}
			}
		}
	}

	if (!InSource.Has(EVoxelStreamingCapability::WorldVisual))
	{
		return;
	}

	const int32 SurfaceRange =
		FMath::Max(
			ProxyRange,
			InSource.View.SurfaceRadiusCells);

	if (bBuildSurface && SurfaceRange > ProxyRange)
	{
		AddAdaptiveTwoDimensionalTiles(
			InSource.Center,
			SurfaceRange,
			// Keep the heightfield below the finite-height volume. Visibility clips
			// only actual replacement geometry, including underwater columns.
			0,
			InViewSettings.SurfaceTileSide,
			1,
			InSource.View.MaximumSurfaceLevel,
			InViewSettings.MaximumSurfaceTiles,
			InSource,
			InSource.View.TargetScreenErrorPixels,
			InOutInterest.Surface);
	}

	const int32 MacroRange =
		FMath::Max(
			SurfaceRange,
			InViewSettings.MacroRadiusCells);

	if (bBuildMacro && MacroRange > SurfaceRange)
	{
		AddAdaptiveTwoDimensionalTiles(
			InSource.Center,
			MacroRange,
			SurfaceRange,
			InViewSettings.MacroTileSide,
			FVoxelMacroTileData::BaseStep,
			InViewSettings.MaximumMacroLevel,
			InViewSettings.MaximumMacroTiles,
			InSource,
			InSource.View.TargetScreenErrorPixels,
			InOutInterest.Macro);
	}
}
