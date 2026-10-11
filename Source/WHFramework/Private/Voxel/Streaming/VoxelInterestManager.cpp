#include "Voxel/Streaming/VoxelInterestManager.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Rendering/VoxelCoverageIndex.h"
#include "Voxel/Rendering/VoxelFinePage.h"
#include "Voxel/Streaming/VoxelTerrainDomainPlanner.h"
#include "Voxel/Task/VoxelTerrainPriority.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

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
	FVoxelFinePageLayout Layout;
	FVoxelFineResidencyLeases Leases;
	if (!InViewSettings.Fine.Validate(Prepared.PlanningError) ||
		!Layout.Initialize(InViewSettings.Fine.MaximumPageLevel, InViewSettings.Fine.ShardSideCells,
			InViewSettings.Fine.FirstMergeDistanceCells, Prepared.PlanningError))
	{
		return Prepared;
	}

	for (const FVoxelStreamingSource& Source :
		InSources)
	{
		FVoxelFineResidency Residency;
		const FVoxelFineResidency* Fine = nullptr;
		if (Source.Has(EVoxelStreamingCapability::FineVisual))
		{
			FVoxelFineMovementDelta Movement;
			if (!Residency.Initialize(Source.Id, InViewSettings.Fine.WindowSettings(), Source.Center,
				InViewSettings.Fine.InteractiveRadiusCells, InViewSettings.Fine.InteractiveVerticalHalfExtentCells,
				InViewSettings.Fine.CollisionGuardRadiusCells, InManifest.Settings.MinZ, InManifest.Settings.MaxZ, Prepared.PlanningError) ||
				!Residency.UpdateMovement(Source.Center, Source.VelocityCellsPerSecond, InViewSettings.Fine.PredictionSeconds,
					Movement, Prepared.PlanningError) ||
				!BuildFineShardDemand(Source, Residency, Prepared, Prepared.PlanningError) ||
				!Layout.SetSource(Source.Id, Source.Center, Prepared.PlanningError) ||
				!Layout.ApplyResidencyDelta(Leases.SetSource(Source.Id, Residency.ResidentShards()), Prepared.PlanningError))
			{
				return Prepared;
			}
			Fine = &Residency;
		}
		AddExactSource(
			Source,
			InManifest,
			InViewSettings,
			Prepared,
			InPrevious, Fine);
		if (!Prepared.PlanningError.IsEmpty())
		{
			return Prepared;
		}

		AddViewSource(
			Source,
			InManifest,
			InViewSettings,
			Prepared, true, true, true);
	}
	while (Layout.IsPending())
	{
		FVoxelFinePageLayoutDelta Delta;
		Layout.Advance(64, Delta);
	}
	Prepared.FineShardPages = Layout.GetPages();
	for (const auto& Pair : Prepared.FineShardPages)
	{
		for (const FVoxelFineShardKey& Shard : Pair.Value.Shards)
		{
			Prepared.FineShardOwners.Add(Shard, Pair.Key);
		}
	}
	return Finalize(InSources, InManifest, InViewSettings, MoveTemp(Prepared));
}

void FVoxelInterestManager::BuildSourceExact(const FVoxelStreamingSource& InSource,
	const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
	FVoxelInterestSet& OutInterest, const FVoxelInterestSet* InPrevious, const FVoxelFineResidency* InFineResidency) const
{
	AddExactSource(InSource, InManifest, InSettings, OutInterest, InPrevious, InFineResidency);
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
	FVoxelInterestSet Result, const FVoxelInterestSet* InPrevious, const bool bInRebuildCoarse, FVoxelTerrainDomainPlanner* InPersistentCoarse) const
{
	if (!Result.PlanningError.IsEmpty() || InManifest.Settings.MinZ >= InManifest.Settings.MaxZ ||
		InManifest.Settings.MinZ % 16 != 0 || InManifest.Settings.MaxZ % 16 != 0)
	{
		if (Result.PlanningError.IsEmpty()) Result.PlanningError = TEXT("世界竖向边界必须为有效16-cell对齐范围");
		return Result;
	}
	int32 FineCount = 0;
	for (const auto& Pair : Result.Exact) FineCount += Pair.Value.bFineRender ? 1 : 0;
	const int64 PageCount64 = int64(FineCount) + Result.FineShardPages.Num();
	if (PageCount64 > MAX_int32)
	{ Result.PlanningError = TEXT("Oversized: Fine Page plan rows"); return Result; }
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> FinePlanLease, FineScratchLease;
	if (Resources)
	{
		const uint64 PlanBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelFinePagePlan>() +
			VoxelBoundedAllocation::SetBytes<FVoxelFinePageKey>(int32(PageCount64)) +
			VoxelBoundedAllocation::MapBytes<FIntVector, FVoxelFinePageKey>(FineCount) +
			VoxelBoundedAllocation::WorkerSharedBytes<TSet<FIntVector>>() + VoxelBoundedAllocation::SetBytes<FIntVector>(FineCount);
		const uint64 ScratchBytes = 2 * VoxelBoundedAllocation::SetBytes<FIntVector>(FineCount) +
			VoxelBoundedAllocation::ArrayBytes<FIntVector>(FineCount);
		const auto Limits = Resources->Snapshot().Limits;
		if (PlanBytes > Limits.InputBytes || ScratchBytes > Limits.WorkingBytes)
		{ Result.PlanningError = TEXT("Oversized: Fine Page plan capacity"); return Result; }
		if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, PlanBytes, FinePlanLease) ||
			!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, FMath::Max<uint64>(1, ScratchBytes), FineScratchLease))
		{ Result.PlanningError = TEXT("CapacityBlocked: Fine Page plan capacity"); return Result; }
	}
	TSet<FIntVector> PlannedFineSections;
	PlannedFineSections.Reserve(FineCount);
	for (const auto& Pair : Result.Exact)
	{
		if (Pair.Value.bFineRender) PlannedFineSections.Add(Pair.Key);
	}
	auto FinePages = FinePlanLease ? VoxelBoundedAllocation::MakeWorkerShared<FVoxelFinePagePlan>(FinePlanLease) :
		MakeShared<FVoxelFinePagePlan, ESPMode::ThreadSafe>();
	FinePages->Pages.Reserve(int32(PageCount64));
	FinePages->SectionOwners.Reserve(FineCount);
	for (const FIntVector& Section : PlannedFineSections)
	{
		const FVoxelFinePageKey Page{Section, 0};
		FinePages->Pages.Add(Page);
		FinePages->SectionOwners.Add(Section, Page);
	}
	for (const auto& Pair : Result.FineShardPages)
	{
		FinePages->Pages.Add(Pair.Key);
	}
	Result.FinePagePlan = FinePages;
	if (bInRebuildCoarse || !InPrevious)
	{
		if (!FVoxelTerrainDomainPlanner::Build(InSources, InManifest, InViewSettings, Result, Result.PlanningError, InPersistentCoarse)) return Result;
	}
	else
	{
		const int64 GeometryRows64 = int64(InPrevious->Geometry.Num()) + FinePages->Pages.Num();
		if (GeometryRows64 > MAX_int32)
		{ Result.PlanningError = TEXT("Oversized: retained coarse geometry rows"); return Result; }
		if (Resources)
		{
			const uint64 Bytes = VoxelBoundedAllocation::SetBytes<FVoxelViewKey>(InPrevious->VoxelProxy.Num()) +
				VoxelBoundedAllocation::SetBytes<FVoxelSurfaceTileKey>(InPrevious->Surface.Num()) +
				VoxelBoundedAllocation::SetBytes<FVoxelMacroTileKey>(InPrevious->Macro.Num()) +
				VoxelBoundedAllocation::SetBytes<FVoxelTerrainSpatialKey>(InPrevious->ResolutionFloorPages.Num()) +
				VoxelBoundedAllocation::MapBytes<FVoxelTerrainSpatialKey, FVoxelTerrainGeometry>(int32(GeometryRows64));
			if (Bytes > Resources->Snapshot().Limits.InputBytes)
			{ Result.PlanningError = TEXT("Oversized: retained coarse geometry capacity"); return Result; }
			if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
				FMath::Max<uint64>(1, Bytes), Result.CoarseCompositionLease))
			{ Result.PlanningError = TEXT("CapacityBlocked: retained coarse geometry capacity"); return Result; }
		}
		Result.CoarseFailedRootCount = InPrevious->CoarseFailedRootCount;
		Result.VoxelProxy = InPrevious->VoxelProxy;
		Result.Surface = InPrevious->Surface;
		Result.Macro = InPrevious->Macro;
		Result.ResolutionFloorPages = InPrevious->ResolutionFloorPages;
		Result.Geometry.Reserve(int32(GeometryRows64));
		for (const auto& Pair : InPrevious->Geometry)
		{
			if (Pair.Key.Representation != EVoxelTerrainRepresentation::Fine) Result.Geometry.Add(Pair.Key, Pair.Value);
		}
		for (const FVoxelFinePageKey& Page : FinePages->Pages)
		{
			const FVoxelTerrainSpatialKey Key{EVoxelTerrainRepresentation::Fine, Page.Coordinate, Page.Level};
			Result.Geometry.Add(Key, {Key, 1, InManifest.Settings.MinZ, InManifest.Settings.MaxZ, InManifest.BaseSampleHash, InManifest.RegistryHash});
		}
		for (auto& Pair : Result.Sources)
		{
			if (const auto* Old = InPrevious->Sources.Find(Pair.Key))
			{
				for (int32 Stage = 0; Stage < 3; ++Stage) Pair.Value.FirstSideCells[Stage] = Old->FirstSideCells[Stage];
			}
		}
	}
	if (!ReserveFinalization(Result, FineCount, Result.PlanningError)) return Result;
	// 精细边界需要真实相邻数据；只增加数据依赖，不扩张碰撞、模拟或可见几何。
	TArray<FIntVector> FineKeys;
	FineKeys.Reserve(FineCount);
	for (const auto& Pair : Result.Exact)
	{
		if (Pair.Value.bFineRender)
		{
			FineKeys.Add(Pair.Key);
		}
	}
	const auto FineSnapshot = FinePlanLease ? VoxelBoundedAllocation::MakeWorkerShared<TSet<FIntVector>>(FinePlanLease) :
		MakeShared<TSet<FIntVector>, ESPMode::ThreadSafe>();
	FineSnapshot->Reserve(FineCount);
	FineSnapshot->Append(FineKeys);
	Result.FineSections = FineSnapshot;
	for (const FIntVector& Key : FineKeys)
	{
		for (const FVoxelStreamingSource& Source : InSources)
		{
			const FVoxelSourceInterest* Contribution = Result.Sources.Find(Source.Id);
			if (Source.Has(EVoxelStreamingCapability::Readiness) && Source.Has(EVoxelStreamingCapability::FineVisual) &&
				Contribution && Contribution->FineDataSections.Contains(Key))
			{
				Result.PlayableFineKeys.Add(Key);
				break;
			}
		}
	}
	// 当前位置与预测位置之间都属于关键范围，前瞻距离不能把当前位置排除在外。
	TSet<FIntVector> CriticalFine;
	CriticalFine.Reserve(FineCount);
	for (const FIntVector& Key : Result.PlayableFineKeys)
	{
		FVoxelExactDemand& Demand = Result.Exact.FindChecked(Key);
		Demand.bMovementCriticalFine = true;
		Demand.bMovementCriticalData = true;
		CriticalFine.Add(Key);
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
	if (Resources) DependencyIndex.BindResources(Resources.ToSharedRef(), Consumer);
	for (const auto& Pair : Result.Geometry)
	{
		FVoxelCoverageRecord Record;
		Record.Geometry = Pair.Value;
		if (!Pair.Key.TryBounds(Pair.Value.WorldMinZ, Pair.Value.WorldMaxZ, Record.Min, Record.Max) ||
			!DependencyIndex.Upsert(Record, Result.PlanningError)) return Result;
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
		DependencyIndex.AnyRecord(Pair.Key * 16 - FIntVector(1), Pair.Key * 16 + FIntVector(17),
			[&Result](const FVoxelCoverageRecord& Record)
			{
				Result.CriticalDependencies.Add(Record.Geometry.Key);
				return false;
			});
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
		if (!Pair.Value.GetFineShards().IsEmpty())
		{
			continue;
		}
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
	if (!RefreshPriorities(InSources, InViewSettings.Fine.InteractiveRadiusCells, Result, Result.PlanningError, Resources, Consumer))
	{
		return Result;
	}
	BuildPublicationInputs(Result, Result.PlanningError);
	return Result;
}

bool FVoxelInterestManager::RefreshPriorities(TConstArrayView<FVoxelStreamingSource> InSources,
	const int32 InP0RadiusCells, FVoxelInterestSet& InOutInterest, FString& OutError,
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources, const FVoxelGenerationConsumerKey& InConsumer)
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> SourceLease;
	if (InResources)
	{
		const uint64 Bytes = 2 * VoxelBoundedAllocation::ArrayBytes<FVoxelStreamingSource>(InSources.Num());
		if (Bytes > InResources->Snapshot().Limits.WorkingBytes)
		{ OutError = TEXT("Oversized: priority Source arrays"); return false; }
		if (!InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Working, FMath::Max<uint64>(1, Bytes), SourceLease))
		{ OutError = TEXT("CapacityBlocked: priority Source arrays"); return false; }
	}
	TArray<FVoxelStreamingSource> DataSources;
	TArray<FVoxelStreamingSource> VisualSources;
	DataSources.Reserve(InSources.Num()); VisualSources.Reserve(InSources.Num());
	for (const FVoxelStreamingSource& Source : InSources)
	{
		if (Source.Has(EVoxelStreamingCapability::Data))
		{
			DataSources.Add(Source);
		}
		if (Source.Has(EVoxelStreamingCapability::FineVisual))
		{
			VisualSources.Add(Source);
		}
	}
	const auto IsCritical = [](const FVoxelExactDemand& Demand)
	{
		return Demand.bWarmupData || Demand.bWarmupCollision || Demand.bMovementCriticalData ||
			Demand.bMovementCriticalFine || Demand.bMovementCriticalCollision;
	};
	const auto RefreshDemand = [&DataSources, &InOutInterest, &OutError, InP0RadiusCells](const FIntVector& Section, FVoxelExactDemand& Demand)
	{
		FVoxelTerrainGeometry Geometry;
		Geometry.Key = {EVoxelTerrainRepresentation::Fine, Section, 0};
		bool bFound = false;
		for (const FVoxelStreamingSource& Source : DataSources)
		{
			const auto* Contribution = InOutInterest.Sources.Find(Source.Id);
			if (!Contribution || !Contribution->DataSections.Contains(Section)) continue;
			FVoxelTerrainTaskRole Role;
			Role.bInteractive = Contribution->F0CoreSections.Contains(Section) ||
				SectionIntersectsRadius(Section, Source.Center, Source.ExactRadius);
			Role.bCollisionGuard = Contribution->CollisionSections.Contains(Section);
			Role.bAheadPrefetch = Contribution->FineDataSections.Contains(Section) && !Contribution->F0CoreSections.Contains(Section);
			FVoxelTerrainServicePriority Candidate;
			if (!FVoxelTerrainPriority::Resolve(Geometry, Role, MakeArrayView(&Source, 1), InP0RadiusCells, Candidate, OutError)) return false;
			if (!bFound || FVoxelTerrainWorkSelection::HigherPriority(Candidate, Demand.ServicePriority))
			{
				Demand.ServicePriority = Candidate;
				bFound = true;
			}
		}
		if (!bFound) { OutError = TEXT("Runtime需求缺少仍有效的Source贡献"); return false; }
		Demand.Priority = Demand.ServicePriority.SourcePriority;
		Demand.DistanceCells = Demand.ServicePriority.DistanceCells;
		Demand.ForwardScore = Demand.ServicePriority.ForwardScore;
		const FVoxelStreamingSource* Winner = DataSources.FindByPredicate([&Demand](const FVoxelStreamingSource& Source)
		{
			return Source.Id == Demand.ServicePriority.SourceStableKey;
		});
		check(Winner);
		const FIntVector Min = Section * InterestSectionSide;
		const FIntVector Max = Min + FIntVector(InterestSectionSide);
		const FVector2D Delta(FMath::Clamp(Winner->Center.X, Min.X, Max.X) - Winner->Center.X,
			FMath::Clamp(Winner->Center.Y, Min.Y, Max.Y) - Winner->Center.Y);
		Demand.HorizontalDistanceCells = Delta.Size();
		return true;
	};
	for (auto& Pair : InOutInterest.Exact)
	{
		if (!RefreshDemand(Pair.Key, Pair.Value))
		{
			return false;
		}
	}
	for (auto& Pair : InOutInterest.Warmup)
	{
		if (!RefreshDemand(Pair.Key, Pair.Value))
		{
			return false;
		}
	}
	InOutInterest.FinePageServicePriorities.Reset();
	InOutInterest.GeometryServicePriorities.Reset();
	InOutInterest.FinePagePriorities.Reset();
	InOutInterest.VoxelProxyPriorities.Reset();
	InOutInterest.SurfacePriorities.Reset();
	InOutInterest.MacroPriorities.Reset();
	InOutInterest.CriticalFinePages.Reset();
	const int32 PageCount = InOutInterest.FinePagePlan ? InOutInterest.FinePagePlan->Pages.Num() : 0;
	// Reset 后必须重新固定哈希容量，不能依赖复制容器恰好保留旧 slack。
	InOutInterest.FinePageServicePriorities.Reserve(PageCount);
	InOutInterest.FinePagePriorities.Reserve(PageCount);
	InOutInterest.CriticalFinePages.Reserve(PageCount);
	InOutInterest.GeometryServicePriorities.Reserve(InOutInterest.Geometry.Num());
	InOutInterest.VoxelProxyPriorities.Reserve(InOutInterest.VoxelProxy.Num());
	InOutInterest.SurfacePriorities.Reserve(InOutInterest.Surface.Num());
	InOutInterest.MacroPriorities.Reserve(InOutInterest.Macro.Num());
	for (auto& Pair : InOutInterest.FineShardDemands)
	{
		const FVoxelFineShardKey& Shard = Pair.Key;
		FVoxelTerrainGeometry Geometry;
		Geometry.Key = {EVoxelTerrainRepresentation::Fine, Shard.Min / Shard.SideCells,
			uint8(FMath::FloorLog2(uint32(Shard.SideCells / 16)))};
		bool bFound = false;
		for (const FVoxelStreamingSource& Source : VisualSources)
		{
			const auto* Contribution = InOutInterest.Sources.Find(Source.Id);
			if (!Contribution || !Contribution->GetFineShards().Contains(Shard)) continue;
			FVoxelTerrainTaskRole Role;
			const auto IntersectsShard = [&Shard](const FIntVector& Section)
			{
				return FVoxelFineSpatial::Intersects(Shard.Bounds(), {Section * 16, (Section + FIntVector(1)) * 16});
			};
			for (const FIntVector& Section : Contribution->F0CoreSections)
			{
				if (IntersectsShard(Section)) { Role.bInteractive = true; break; }
			}
			if (!Role.bInteractive)
			{
				for (const FIntVector& Section : Contribution->CollisionSections)
				{
					if (IntersectsShard(Section)) { Role.bCollisionGuard = true; break; }
				}
			}
			FVoxelTerrainServicePriority Candidate;
			if (!FVoxelTerrainPriority::Resolve(Geometry, Role, MakeArrayView(&Source, 1), InP0RadiusCells, Candidate, OutError)) return false;
			if (!bFound || FVoxelTerrainWorkSelection::HigherPriority(Candidate, Pair.Value.ServicePriority))
			{
				Pair.Value.ServicePriority = Candidate;
				bFound = true;
			}
		}
		if (!bFound) { OutError = TEXT("Fine Shard需求缺少空间租约Source"); return false; }
	}
	if (InOutInterest.FinePagePlan)
	{
		for (const FVoxelFinePageKey& Page : InOutInterest.FinePagePlan->Pages)
		{
			FVoxelTerrainServicePriority Priority;
			bool bFound = false;
			if (Page.Level == 0)
			{
				if (const auto* Demand = InOutInterest.Exact.Find(Page.Coordinate))
				{
					Priority = Demand->ServicePriority;
					bFound = true;
					if (IsCritical(*Demand)) InOutInterest.CriticalFinePages.Add(Page);
				}
			}
			else if (const auto* Layout = InOutInterest.FineShardPages.Find(Page))
			{
				for (const FVoxelFineShardKey& Shard : Layout->Shards)
				{
					const auto& Demand = InOutInterest.FineShardDemands.FindChecked(Shard);
					if (!bFound || FVoxelTerrainWorkSelection::HigherPriority(Demand.ServicePriority, Priority))
					{
						Priority = Demand.ServicePriority;
						bFound = true;
					}
					if (Demand.bCriticalDependency) InOutInterest.CriticalFinePages.Add(Page);
				}
			}
			if (!bFound) { OutError = TEXT("Fine Page缺少F0或Shard布局所有权"); return false; }
			InOutInterest.FinePageServicePriorities.Add(Page, Priority);
			InOutInterest.FinePagePriorities.Add(Page, Priority.SourcePriority);
		}
	}
	for (const auto& Pair : InOutInterest.Geometry)
	{
		if (Pair.Key.Representation == EVoxelTerrainRepresentation::Fine)
		{
			const auto* Priority = InOutInterest.FinePageServicePriorities.Find({Pair.Key.Coordinate, Pair.Key.Level});
			if (!Priority) { OutError = TEXT("Fine Geometry缺少已接受Page布局"); return false; }
			InOutInterest.GeometryServicePriorities.Add(Pair.Key, *Priority);
			continue;
		}
		FVoxelTerrainTaskRole Role;
		Role.bCollisionGuard = InOutInterest.CriticalDependencies.Contains(Pair.Key);
		FVoxelTerrainServicePriority Priority;
		if (!FVoxelTerrainPriority::Resolve(Pair.Value, Role, VisualSources, InP0RadiusCells, Priority, OutError))
		{
			return false;
		}
		InOutInterest.GeometryServicePriorities.Add(Pair.Key, Priority);
		switch (Pair.Key.Representation)
		{
		case EVoxelTerrainRepresentation::VoxelProxy:
			InOutInterest.VoxelProxyPriorities.Add({Pair.Key.Coordinate, Pair.Key.Level}, Priority.SourcePriority);
			break;
		case EVoxelTerrainRepresentation::Surface:
			InOutInterest.SurfacePriorities.Add({FIntPoint(Pair.Key.Coordinate.X, Pair.Key.Coordinate.Y), Pair.Key.Level}, Priority.SourcePriority);
			break;
		case EVoxelTerrainRepresentation::Macro:
			InOutInterest.MacroPriorities.Add({FIntPoint(Pair.Key.Coordinate.X, Pair.Key.Coordinate.Y), Pair.Key.Level}, Priority.SourcePriority);
			break;
		default:
			break;
		}
	}
	for (FVoxelViewAdmission& Admission : InOutInterest.Admissions)
	{
		const FVoxelTerrainServicePriority* Priority = nullptr;
		switch (Admission.Kind)
		{
		case EVoxelViewAdmissionKind::Fine:
			Priority = &InOutInterest.Exact.FindChecked(Admission.FineKey).ServicePriority;
			break;
		case EVoxelViewAdmissionKind::VoxelProxy:
			Priority = InOutInterest.GeometryServicePriorities.Find({EVoxelTerrainRepresentation::VoxelProxy, Admission.ProxyKey.Coordinate, Admission.ProxyKey.Level});
			break;
		case EVoxelViewAdmissionKind::Surface:
			Priority = InOutInterest.GeometryServicePriorities.Find({EVoxelTerrainRepresentation::Surface,
				FIntVector(Admission.SurfaceKey.Coordinate.X, Admission.SurfaceKey.Coordinate.Y, 0), Admission.SurfaceKey.Level});
			break;
		case EVoxelViewAdmissionKind::Macro:
			Priority = InOutInterest.GeometryServicePriorities.Find({EVoxelTerrainRepresentation::Macro,
				FIntVector(Admission.MacroKey.Coordinate.X, Admission.MacroKey.Coordinate.Y, 0), Admission.MacroKey.Level});
			break;
		}
		if (!Priority)
		{
			OutError = TEXT("Admission缺少已接受的空间优先级");
			return false;
		}
		Admission.Priority = Priority->SourcePriority;
		Admission.DistanceCells = Priority->DistanceCells;
	}
	InOutInterest.Admissions.Sort();
	InOutInterest.AdmissionPriorities.Reset();
	InOutInterest.AdmissionPriorities.Reserve(InOutInterest.Admissions.Num());
	for (TArray<int32>& Lane : InOutInterest.AdmissionLanes)
	{
		Lane.Reset();
		Lane.Reserve(InOutInterest.Admissions.Num());
	}
	for (int32 Index = 0; Index < InOutInterest.Admissions.Num(); ++Index)
	{
		const FVoxelViewAdmission& Admission = InOutInterest.Admissions[Index];
		if (InOutInterest.AdmissionPriorities.IsEmpty() || InOutInterest.AdmissionPriorities.Last() != Admission.Priority)
		{
			InOutInterest.AdmissionPriorities.Add(Admission.Priority);
		}
		InOutInterest.AdmissionLanes[static_cast<uint8>(Admission.Kind)].Add(Index);
	}
	SortExactDemands(InOutInterest);
	OutError.Reset();
	return SettleFinalization(InOutInterest, OutError);
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

			if (A.ServicePriority != B.ServicePriority)
			{
				return FVoxelTerrainWorkSelection::HigherPriority(A.ServicePriority, B.ServicePriority);
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

bool FVoxelInterestManager::ReserveDemands(FVoxelInterestSet& InOutInterest, FVoxelSourceInterest& InOutSource,
	const int32 InAdditionalKeys, FString& OutError) const
{
	if (!Resources) return true;
	const int64 DemandCount64 = int64(InOutInterest.Exact.Num()) + InAdditionalKeys;
	const int64 SourceCount64 = int64(InOutSource.DataSections.Num()) + InAdditionalKeys;
	if (InAdditionalKeys < 0 || DemandCount64 > MAX_int32 || SourceCount64 > MAX_int32)
	{ OutError = TEXT("Oversized: exact demand rows"); return false; }
	const int32 DemandCount = int32(DemandCount64), SourceCount = int32(SourceCount64);
	const uint64 OldBytes = InOutInterest.GetAllocatedBytes();
	const uint64 OldTables = InOutInterest.Exact.GetAllocatedSize() + InOutInterest.Warmup.GetAllocatedSize();
	const uint64 OldSource = InOutSource.DataSections.GetAllocatedSize() + InOutSource.FineDataSections.GetAllocatedSize() +
		InOutSource.F0CoreSections.GetAllocatedSize() + InOutSource.CollisionSections.GetAllocatedSize() + InOutSource.SimulationSections.GetAllocatedSize();
	const uint64 NewTables = FMath::Max<uint64>(InOutInterest.Exact.GetAllocatedSize(), VoxelBoundedAllocation::MapBytes<FIntVector, FVoxelExactDemand>(DemandCount)) +
		FMath::Max<uint64>(InOutInterest.Warmup.GetAllocatedSize(), VoxelBoundedAllocation::MapBytes<FIntVector, FVoxelExactDemand>(DemandCount));
	const uint64 PerSet = VoxelBoundedAllocation::SetBytes<FIntVector>(SourceCount);
	const uint64 NewSource = FMath::Max<uint64>(PerSet, InOutSource.DataSections.GetAllocatedSize()) +
		FMath::Max<uint64>(PerSet, InOutSource.FineDataSections.GetAllocatedSize()) + FMath::Max<uint64>(PerSet, InOutSource.F0CoreSections.GetAllocatedSize()) +
		FMath::Max<uint64>(PerSet, InOutSource.CollisionSections.GetAllocatedSize()) + FMath::Max<uint64>(PerSet, InOutSource.SimulationSections.GetAllocatedSize());
	const uint64 NewBytes = OldBytes - OldTables - OldSource + NewTables + NewSource;
	const uint64 Peak = OldBytes + NewBytes;
	if (Peak > Resources->Snapshot().Limits.InputBytes)
	{ OutError = TEXT("Oversized: exact demand allocation peak"); return false; }
	if ((!InOutInterest.DemandLease && !Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
		FMath::Max<uint64>(1, Peak), InOutInterest.DemandLease)) || !InOutInterest.DemandLease->Resize(FMath::Max<uint64>(1, Peak)))
	{ OutError = TEXT("CapacityBlocked: exact demand allocation peak"); return false; }
	InOutInterest.Exact.Reserve(DemandCount); InOutInterest.Warmup.Reserve(DemandCount);
	InOutSource.DataSections.Reserve(SourceCount); InOutSource.FineDataSections.Reserve(SourceCount);
	InOutSource.F0CoreSections.Reserve(SourceCount); InOutSource.CollisionSections.Reserve(SourceCount); InOutSource.SimulationSections.Reserve(SourceCount);
	check(InOutInterest.GetAllocatedBytes() <= NewBytes);
	const bool bResized = InOutInterest.DemandLease->Resize(FMath::Max<uint64>(1, NewBytes));
	check(bResized);
	OutError.Reset(); return true;
}

void FVoxelInterestManager::AddExactSource(
	const FVoxelStreamingSource& InSource,
	const FVoxelWorldManifest& InManifest,
	const FVoxelViewSettings& InViewSettings,
	FVoxelInterestSet& InOutInterest,
	const FVoxelInterestSet* InPrevious, const FVoxelFineResidency* InFineResidency) const
{
	if (Resources)
	{
		const uint64 Bytes = InOutInterest.GetAllocatedBytes() +
			VoxelBoundedAllocation::MapBytes<FGuid, FVoxelSourceInterest>(InOutInterest.Sources.Num() + 1);
		if ((!InOutInterest.DemandLease && !Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
			Bytes, InOutInterest.DemandLease)) || (InOutInterest.DemandLease && !InOutInterest.DemandLease->Resize(Bytes)))
		{ InOutInterest.PlanningError = TEXT("CapacityBlocked: exact Source table"); return; }
		InOutInterest.Sources.Reserve(InOutInterest.Sources.Num() + 1);
	}
	FVoxelSourceInterest& SourceInterest = InOutInterest.Sources.FindOrAdd(InSource.Id);
	SourceInterest.Source = InSource;
	if (!InSource.Has(EVoxelStreamingCapability::Data))
	{
		return;
	}
	FString Error;
	TSet<FIntVector> Core;
	TSet<FIntVector> Guard;
	TSet<FIntVector> ExplicitData;
	FVoxelFineSpatialCapacity ExplicitCapacity{Resources, Consumer, EVoxelTerrainResourcePool::Working};
	FVoxelFineSpatialCapacity GuardCapacity = ExplicitCapacity;
	const bool bFine = InSource.Has(EVoxelStreamingCapability::FineVisual);
	if (bFine)
	{
		if (!InFineResidency)
		{
			InOutInterest.PlanningError = TEXT("Fine需求缺少InterestRuntime持有的驻留窗口");
			return;
		}
	}
	else if (InSource.Has(EVoxelStreamingCapability::Collision) && InSource.CollisionRadius > 0)
	{
		const FVector Start(InSource.Center);
		const FVector End = Start + InSource.VelocityCellsPerSecond * InViewSettings.Fine.PredictionSeconds;
		if (!FVoxelFineSpatial::GatherSweptGuard(Start, End, InSource.CollisionRadius,
			InManifest.Settings.MinZ, InManifest.Settings.MaxZ, 262144, Guard, Error, &GuardCapacity))
		{
			InOutInterest.PlanningError = MoveTemp(Error);
			return;
		}
	}
	if (InSource.ExactRadius >= 0 && !FVoxelFineSpatial::GatherCore(InSource.Center,
		InSource.ExactRadius, FMath::Max(1, InSource.ExactRadius),
		InManifest.Settings.MinZ, InManifest.Settings.MaxZ, ExplicitData, Error, &ExplicitCapacity))
	{
		InOutInterest.PlanningError = MoveTemp(Error);
		return;
	}
	const TSet<FIntVector>& CoreKeys = bFine ? InFineResidency->GetCore() : Core;
	const TSet<FIntVector>& GuardKeys = bFine && InSource.Has(EVoxelStreamingCapability::Collision) ? InFineResidency->GetGuard() : Guard;
	const int64 MaximumKeys = int64(CoreKeys.Num()) * 7 + GuardKeys.Num() + ExplicitData.Num();
	if (MaximumKeys > MAX_int32 || !ReserveDemands(InOutInterest, SourceInterest, int32(MaximumKeys), Error))
	{ InOutInterest.PlanningError = MaximumKeys > MAX_int32 ? TEXT("Oversized: exact demand rows") : MoveTemp(Error); return; }
	const bool bReadiness = InSource.Has(EVoxelStreamingCapability::Readiness);
	const auto AddData = [&InOutInterest, &SourceInterest](const FIntVector& Key) -> FVoxelExactDemand&
	{
		SourceInterest.DataSections.Add(Key);
		FVoxelExactDemand& Demand = InOutInterest.Exact.FindOrAdd(Key);
		Demand.bData = true;
		return Demand;
	};
	for (const FIntVector& Key : ExplicitData)
	{
		FVoxelExactDemand& Demand = AddData(Key);
		Demand.bExact = true;
		Demand.bWarmupData |= bReadiness;
	}
	for (const FIntVector& Key : CoreKeys)
	{
		SourceInterest.F0CoreSections.Add(Key);
		FVoxelExactDemand& Demand = AddData(Key);
		Demand.bExact = true;
		Demand.bFineRender = true;
		Demand.bFineData = true;
		Demand.bMovementCriticalFine = true;
		Demand.bMovementCriticalData = true;
		Demand.bWarmupData |= bReadiness;
		SourceInterest.FineDataSections.Add(Key);
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			for (const int32 Sign : {-1, 1})
			{
				FIntVector Neighbor = Key;
				Neighbor[Axis] += Sign;
				if (int64(Neighbor.Z) * 16 < InManifest.Settings.MaxZ &&
					(int64(Neighbor.Z) + 1) * 16 > InManifest.Settings.MinZ)
				{
					AddData(Neighbor).bMovementCriticalData = true;
					SourceInterest.FineDataSections.Add(Neighbor);
				}
			}
		}
	}
	for (const FIntVector& Key : GuardKeys)
	{
		FVoxelExactDemand& Demand = AddData(Key);
		Demand.bCollision = true;
		Demand.bMovementCriticalCollision = true;
		Demand.bMovementCriticalData = true;
		Demand.bWarmupData |= bReadiness;
		Demand.bWarmupCollision |= bReadiness;
		SourceInterest.CollisionSections.Add(Key);
	}
	if (!BuildSimulationDemand(InSource, InManifest, InOutInterest, Error))
	{
		InOutInterest.PlanningError = MoveTemp(Error);
		return;
	}
	if (bReadiness)
	{
		for (const FIntVector& Key : SourceInterest.DataSections)
		{
			const FVoxelExactDemand& Demand = InOutInterest.Exact.FindChecked(Key);
			if (!Demand.bWarmupData && !Demand.bWarmupCollision)
			{
				continue;
			}
			FVoxelExactDemand& Warmup = InOutInterest.Warmup.FindOrAdd(Key);
			Warmup.bData = true;
			Warmup.bWarmupData = true;
			Warmup.bWarmupCollision |= SourceInterest.CollisionSections.Contains(Key);
			Warmup.bFineRender |= CoreKeys.Contains(Key);
		}
	}
}

bool FVoxelInterestManager::BuildSimulationDemand(const FVoxelStreamingSource& InSource,
	const FVoxelWorldManifest& InManifest, FVoxelInterestSet& InOutInterest, FString& OutError) const
{
	if (!InSource.Has(EVoxelStreamingCapability::Simulation) || InSource.SimulationRadius <= 0)
	{
		return true;
	}
	TSet<FIntVector> Sections;
	FVoxelFineSpatialCapacity Capacity{Resources, Consumer, EVoxelTerrainResourcePool::Working};
	if (!FVoxelFineSpatial::GatherSweptGuard(FVector(InSource.Center), FVector(InSource.Center), InSource.SimulationRadius,
		InManifest.Settings.MinZ, InManifest.Settings.MaxZ, 262144, Sections, OutError, &Capacity))
	{
		return false;
	}
	FVoxelSourceInterest& SourceInterest = InOutInterest.Sources.FindOrAdd(InSource.Id);
	if (!ReserveDemands(InOutInterest, SourceInterest, Sections.Num(), OutError)) return false;
	SourceInterest.Source = InSource;
	SourceInterest.SimulationSections.Append(Sections);
	for (const FIntVector& Key : Sections)
	{
		SourceInterest.DataSections.Add(Key);
		SourceInterest.CollisionSections.Add(Key);
		FVoxelExactDemand& Demand = InOutInterest.Exact.FindOrAdd(Key);
		Demand.bData = true;
		Demand.bSimulation = true;
		Demand.bCollision = true;
	}
	return true;
}

bool FVoxelInterestManager::BuildFineShardDemand(const FVoxelStreamingSource& InSource,
	const FVoxelFineResidency& InResidency, FVoxelInterestSet& InOutInterest, FString& OutError) const
{
	FVoxelSourceInterest& SourceInterest = InOutInterest.Sources.FindOrAdd(InSource.Id);
	SourceInterest.Source = InSource;
	SourceInterest.FineResidency = InResidency.Capture(OutError);
	if (!SourceInterest.FineResidency) return false;
	SourceInterest.FineBounds = SourceInterest.GetCurrentWindow().Bounds;
	if (SourceInterest.GetPendingWindow().IsSet())
	{
		SourceInterest.FineBounds.Min = SourceInterest.FineBounds.Min.ComponentMin(SourceInterest.GetPendingWindow()->Bounds.Min);
		SourceInterest.FineBounds.Max = SourceInterest.FineBounds.Max.ComponentMax(SourceInterest.GetPendingWindow()->Bounds.Max);
	}
	for (const FVoxelFineShardKey& Key : SourceInterest.GetFineShards())
	{
		if (!Key.Validate(OutError))
		{
			return false;
		}
		FVoxelFineShardDemand& Demand = InOutInterest.FineShardDemands.FindOrAdd(Key);
		++Demand.SourceReferences;
		const FIntVector First = Key.Min / 16;
		const int32 Count = Key.SideCells / 16;
		for (int32 Z = 0; Z < Count && !Demand.bCriticalDependency; ++Z)
		{
			for (int32 Y = 0; Y < Count && !Demand.bCriticalDependency; ++Y)
			{
				for (int32 X = 0; X < Count; ++X)
				{
					const FIntVector Section = First + FIntVector(X, Y, Z);
					if (InResidency.GetCore().Contains(Section) || InResidency.GetGuard().Contains(Section))
					{
						Demand.bCriticalDependency = true;
						break;
					}
				}
			}
		}
	}
	OutError.Reset();
	return true;
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
