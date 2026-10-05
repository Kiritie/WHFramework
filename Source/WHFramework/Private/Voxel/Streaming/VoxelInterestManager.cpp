#include "Voxel/Streaming/VoxelInterestManager.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Rendering/VoxelCoverageIndex.h"
#include "Voxel/Rendering/VoxelFinePage.h"
#include "Voxel/Streaming/VoxelTerrainDomainPlanner.h"
#include "Voxel/Streaming/VoxelFineDomain.h"

const FVoxelTerrainGeometry* FVoxelInterestSet::FindGeometry(const FVoxelViewKey& InKey) const
{
	return Geometry.Find({EVoxelTerrainRepresentation::VoxelProxy, InKey.Coordinate, InKey.Level});
}

const FVoxelTerrainGeometry* FVoxelInterestSet::FindGeometry(const FVoxelSurfaceTileKey& InKey) const
{
	return Geometry.Find({EVoxelTerrainRepresentation::Surface, FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0), InKey.Level});
}

const FVoxelTerrainGeometry* FVoxelInterestSet::FindGeometry(const FVoxelMacroTileKey& InKey) const
{
	return Geometry.Find({EVoxelTerrainRepresentation::Macro, FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0), InKey.Level});
}

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


}

FVoxelInterestSet FVoxelInterestManager::Compute(
	TConstArrayView<FVoxelStreamingSource> InSources,
	const FVoxelWorldManifest& InManifest,
	const FVoxelViewSettings& InViewSettings,
	const FVoxelInterestSet* InPrevious) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ViewInterestPlan);

	FVoxelInterestSet Prepared;
	if (!VoxelViewLod::ValidateTopology(InViewSettings.Lod, Prepared.PlanningError)) return Prepared;

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
	if (!VoxelFineDomain::CompleteInitialFamilies(InSources, InManifest, InViewSettings, Result, Result.PlanningError))
	{
		return Result;
	}
	TSet<FIntVector> PlannedFineSections;
	for (const auto& Pair : Result.Exact)
	{
		if (Pair.Value.bFineRender) PlannedFineSections.Add(Pair.Key);
	}
	auto FinePages = MakeShared<FVoxelFinePagePlan, ESPMode::ThreadSafe>();
	if (!FVoxelFinePagePlanner::Build(PlannedFineSections, InSources, InViewSettings, *FinePages, Result.PlanningError))
	{
		return Result;
	}
	Result.FinePagePlan = FinePages;
	if (!FVoxelTerrainDomainPlanner::Build(InSources, InManifest, InViewSettings, Result, Result.PlanningError)) return Result;
	// 预测只预取真实数据；FineWanted 继续由当前位置的 Fine 圆柱决定。
	for (const FVoxelStreamingSource& Source : InSources)
	{
		if (!Source.Has(EVoxelStreamingCapability::Data) || !Source.Has(EVoxelStreamingCapability::FineVisual) ||
			Source.VelocityCellsPerSecond.IsNearlyZero() || Source.View.FinePredictionSeconds <= 0.0f) continue;
		const FVector Current(Source.Center);
		const FVector Predicted = Current + Source.VelocityCellsPerSecond * Source.View.FinePredictionSeconds;
		const double Radius = FMath::Max(Source.View.FineLevel0RadiusCells, Source.CollisionRadius) + InterestSectionSide;
		const FVector Min = Current.ComponentMin(Predicted) - FVector(Radius);
		const FVector Max = Current.ComponentMax(Predicted) + FVector(Radius);
		const FIntVector MinSection(FMath::FloorToInt(Min.X / 16), FMath::FloorToInt(Min.Y / 16),
			FMath::Max(FMath::FloorToInt(Min.Z / 16), VoxelGeneration::FloorDivide(InManifest.Settings.MinZ, 16)));
		const FIntVector MaxSection(FMath::FloorToInt(Max.X / 16), FMath::FloorToInt(Max.Y / 16),
			FMath::Min(FMath::FloorToInt(Max.Z / 16), VoxelGeneration::FloorDivide(InManifest.Settings.MaxZ - 1, 16)));
		for (int32 Z = MinSection.Z; Z <= MaxSection.Z; ++Z)
		{
			for (int32 Y = MinSection.Y; Y <= MaxSection.Y; ++Y)
			{
				for (int32 X = MinSection.X; X <= MaxSection.X; ++X)
				{
					const FIntVector Key(X, Y, Z);
					const FVector Center(Key * 16 + FIntVector(8));
					if (FMath::PointDistToSegmentSquared(Center, Current, Predicted) > Radius * Radius) continue;
					FVoxelExactDemand& Demand = Result.Exact.FindOrAdd(Key);
					Demand.bData = true;
					Demand.bMovementCriticalData = true;
					Demand.Priority = FMath::Min(Demand.Priority, Source.GetSchedulingPriority());
					Demand.DistanceCells = FMath::Min(Demand.DistanceCells, FVector::Distance(Center, Current));
				}
			}
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
			const double PlayableRadius = Source.View.FineRadiusCells * InViewSettings.Readiness.PlayableFineRadiusFraction;
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
	for (const FIntVector& Key : Result.PlayableFineKeys)
	{
		FVoxelExactDemand& Demand = Result.Exact.FindChecked(Key);
		Demand.bMovementCriticalFine = true;
		Demand.bMovementCriticalData = true;
		CriticalFine.Add(Key);
	}
	for (const FVoxelStreamingSource& Source : InSources)
	{
		if (!Source.Has(EVoxelStreamingCapability::FineVisual) ||
			Source.Has(EVoxelStreamingCapability::LocalRefinement) ||
			VoxelViewLod::MovementCriticalFineRadius(Source) <= 0) continue;
		const FVector Current(Source.Center);
		const FVector Predicted = Current + Source.VelocityCellsPerSecond * Source.View.FinePredictionSeconds;
		const double RadiusSquared = FMath::Square(static_cast<double>(VoxelViewLod::MovementCriticalFineRadius(Source)));
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
				DistanceSquared <= FMath::Square(static_cast<double>(Source.View.FineRadiusCells)))
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
		const int32 Side = 16 << Key.Level;
		const FVector Center((Key.Coordinate.X + 0.5) * Side, (Key.Coordinate.Y + 0.5) * Side, 0.0);
		const FVector Min(Key.Coordinate.X * Side, Key.Coordinate.Y * Side, 0.0);
		A.Priority = ViewPriority(FBox(Min, Min + FVector(Side, Side, 0.0)), true,
			&FVoxelStreamingSourceView::SurfaceRadiusCells);
		A.DistanceCells = Distance(FBox(Min, Min + FVector(Side, Side, 0.0)), true, false, A.Priority);
		Result.SurfacePriorities.Add(Key, A.Priority);
	}
	for (const FVoxelMacroTileKey& Key : Result.Macro)
	{
		FVoxelViewAdmission& A = Result.Admissions.AddDefaulted_GetRef();
		A.Kind = EVoxelViewAdmissionKind::Macro;
		A.MacroKey = Key;
		const int32 Side = 16 << Key.Level;
		const FVector Center((Key.Coordinate.X + 0.5) * Side, (Key.Coordinate.Y + 0.5) * Side, 0.0);
		const FVector Min(Key.Coordinate.X * Side, Key.Coordinate.Y * Side, 0.0);
		A.Priority = ViewPriority(FBox(Min, Min + FVector(Side, Side, 0.0)), true,
			&FVoxelStreamingSourceView::MacroRadiusCells);
		A.DistanceCells = Distance(FBox(Min, Min + FVector(Side, Side, 0.0)), true, false, A.Priority);
		Result.MacroPriorities.Add(Key, A.Priority);
	}
	FVoxelCoverageIndex DependencyIndex;
	for (const auto& Pair : Result.Geometry)
	{
		auto Record = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
		Record->Geometry = Pair.Value;
		FString Error;
		if (Pair.Key.TryBounds(Pair.Value.WorldMinZ, Pair.Value.WorldMaxZ, Record->Min, Record->Max))
			DependencyIndex.Upsert(Record, Error);
	}
	for (const auto& Pair : Result.Exact)
	{
		const auto& Demand = Pair.Value;
		const bool bCritical =
			Demand.bWarmupData || Demand.bWarmupCollision || Demand.bMovementCriticalData || Demand.bMovementCriticalCollision || Demand.bMovementCriticalFine;
		if (const auto* Page = Result.FinePagePlan->SectionOwners.Find(Pair.Key))
		{
			int32* Priority = Result.FinePagePriorities.Find(*Page);
			if (Priority)
			{
				*Priority = FMath::Min(*Priority, Demand.Priority);
			}
			else
			{
				Result.FinePagePriorities.Add(*Page, Demand.Priority);
			}
			if (bCritical)
			{
				Result.CriticalFinePages.Add(*Page);
			}
		}
		if (!bCritical)
		{
			continue;
		}
		FVoxelCoverageLocalSnapshot Local;
		DependencyIndex.Capture(Pair.Key * 16 - FIntVector(1), Pair.Key * 16 + FIntVector(17), Local);
		for (const auto& Record : Local.Records)
			Result.CriticalDependencies.Add(Record->Geometry.Key);
	}
	Result.Admissions.Sort();
	for (const auto& Admission : Result.Admissions)
		if (Result.AdmissionPriorities.IsEmpty() || Result.AdmissionPriorities.Last() != Admission.Priority)
			Result.AdmissionPriorities.Add(Admission.Priority);
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
	auto Inputs = MakeShared<FVoxelTerrainPublicationInputs, ESPMode::ThreadSafe>();
	Result.Geometry.GenerateValueArray(Inputs->Geometry);
	Inputs->Roots = Result.TerrainPlan.Roots.Array();
	Inputs->Fine = Result.FineSections->Array();
	Inputs->Proxy = Result.VoxelProxy.Array();
	Inputs->Surface = Result.Surface.Array();
	Inputs->Macro = Result.Macro.Array();
	Inputs->FinePages = Result.FinePagePlan->Pages.Array();
	for (const auto& Pair : Result.FinePagePlan->SectionOwners) Inputs->FineOwners.Add(Pair);
	Inputs->Admissions = Result.Admissions;
	Inputs->AdmissionPriorities = Result.AdmissionPriorities;
	for (int32 Lane = 0; Lane < 4; ++Lane)
	{
		Inputs->AdmissionLanes[Lane] = Result.AdmissionLanes[Lane];
	}
	Result.PublicationInputs = MoveTemp(Inputs);
	return Result;
}

void FVoxelInterestManager::SortExactDemands(FVoxelInterestSet& Result)
{
	const auto IsCritical = [](const FVoxelExactDemand& Demand)
	{
		return Demand.bWarmupData || Demand.bWarmupCollision || Demand.bMovementCriticalData || Demand.bMovementCriticalFine ||
			   Demand.bMovementCriticalCollision;
	};

	Result.Exact.GetKeys(Result.ExactOrder);
	Result.ExactOrder.Sort(
		[&Result, &IsCritical](const FIntVector& Left, const FIntVector& Right)
		{
			const FVoxelExactDemand& A = Result.Exact.FindChecked(Left);
			const FVoxelExactDemand& B = Result.Exact.FindChecked(Right);

			const bool bCriticalA = IsCritical(A);
			const bool bCriticalB = IsCritical(B);
			if (bCriticalA != bCriticalB)
			{
				return bCriticalA;
			}

			if (A.Priority != B.Priority)
			{
				return A.Priority < B.Priority;
			}

			const bool bGameplayA = A.bExact || A.bCollision || A.bSimulation || A.bWarmupData;
			const bool bGameplayB = B.bExact || B.bCollision || B.bSimulation || B.bWarmupData;
			if (bGameplayA != bGameplayB)
			{
				return bGameplayA;
			}

			if (A.DistanceCells != B.DistanceCells)
			{
				return A.DistanceCells < B.DistanceCells;
			}
			if (A.ForwardScore != B.ForwardScore)
			{
				return A.ForwardScore > B.ForwardScore;
			}
			if (Left.X != Right.X)
			{
				return Left.X < Right.X;
			}
			if (Left.Y != Right.Y)
			{
				return Left.Y < Right.Y;
			}
			return Left.Z < Right.Z;
		});

	for (TArray<int32>& Lane : Result.ExactAdmissionLanes)
	{
		Lane.Reset();
	}

	for (int32 Index = 0; Index < Result.ExactOrder.Num(); ++Index)
	{
		const FVoxelExactDemand& Demand = Result.Exact.FindChecked(Result.ExactOrder[Index]);
		Result.ExactAdmissionLanes[IsCritical(Demand) ? 0 : 1].Add(Index);
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
		InSource.View.FineVerticalHalfExtentCells > 0;

	const int32 FineRadius = bWantsFine ? FMath::Max(0, InSource.View.FineRadiusCells) : 0;
	const int32 FineVerticalRadius =
		bWantsFine
			? FMath::Max(0, InSource.View.FineVerticalHalfExtentCells)
			: 0;

	const int32 WarmupDataRadius =
		FMath::Min(
			FMath::RoundToInt(FMath::Max(0, ExactRadius) * (bPrewarm ? 1.0f : FMath::Clamp(InViewSettings.Readiness.WarmupDataRadiusFraction, 0.0f, 1.0f))),
			ExactRadius);

	const int32 WarmupCollisionRadius =
		InSource.Has(EVoxelStreamingCapability::Collision)
			? FMath::Min(
				FMath::RoundToInt(CollisionRadius * (bPrewarm ? 1.0f : FMath::Clamp(InViewSettings.Readiness.WarmupCollisionRadiusFraction, 0.0f, 1.0f))),
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
		FMath::Max(FineRetainVerticalRadius, FMath::Max3(ExactRadius, CollisionRadius, SimulationRadius)), InterestSectionSide);

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

				const bool bFineRender =
					bWantsFine &&
					IsInsideFineVolume(
						FIntVector(FMath::Max(0, FMath::Abs(Delta.X) - InterestSectionSide / 2),
							FMath::Max(0, FMath::Abs(Delta.Y) - InterestSectionSide / 2),
							FMath::Max(0, FMath::Abs(Delta.Z) - InterestSectionSide / 2)),
						FineRadius,
						FineVerticalRadius);

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
	// Coarse domains are computed together in Finalize from the actual preceding frontier.
}
