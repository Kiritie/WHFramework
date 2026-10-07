#include "Voxel/Streaming/VoxelTerrainDomainPlanner.h"

#include "Voxel/Rendering/VoxelCoverageIndex.h"
#include "Voxel/Rendering/VoxelCoveragePartition.h"
#include "Voxel/Rendering/VoxelTerrainStagePlanner.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"

struct FVoxelTerrainDomainSourceBatch
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	TArray<FVoxelTerrainStageSource> Values;
};

void FVoxelTerrainDomainPlanner::BindResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer)
{
	for (auto& Stage : Stages) { Stage.Resources = InResources; Stage.Consumer = InConsumer; }
}

bool FVoxelTerrainDomainPlanner::ApplySourceDelta(const EVoxelTerrainRepresentation InRepresentation,
	TConstArrayView<FVoxelTerrainStageSource> InSources, FString& OutError)
{
	const int32 Stage = int32(InRepresentation) - int32(EVoxelTerrainRepresentation::VoxelProxy);
	if (Stage < 0 || Stage >= 3) { OutError = TEXT("Invalid coarse domain"); return false; }
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainDomainSourceBatch>() +
		VoxelBoundedAllocation::ArrayBytes<FVoxelTerrainStageSource>(InSources.Num());
	TSharedPtr<FVoxelTerrainDomainSourceBatch, ESPMode::ThreadSafe> Candidate;
	if (Stages[Stage].Resources)
	{
		if (!Stages[Stage].Resources->Acquire(Stages[Stage].Consumer, EVoxelTerrainResourcePool::Input, Bytes, Lease))
		{ OutError = TEXT("CapacityBlocked: coarse source inputs"); return false; }
		Candidate = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainDomainSourceBatch>(Lease);
	}
	else Candidate = MakeShared<FVoxelTerrainDomainSourceBatch, ESPMode::ThreadSafe>();
	Candidate->Lease = MoveTemp(Lease); Candidate->Values.Reserve(InSources.Num()); Candidate->Values.Append(InSources);
	Sources[Stage] = MoveTemp(Candidate);
	if (InSources.IsEmpty())
	{
		Stages[Stage].Root.Reset(); Stages[Stage].bCapacityBlocked = false; Stages[Stage].FailedRoots = 0;
	}
	return true;
}

bool FVoxelTerrainDomainPlanner::AdvanceDirtyRoots(const EVoxelTerrainRepresentation InRepresentation, const uint8 InMaximumLevel,
	const int32 InMaximumSamples, const int32 InMinZ, const int32 InMaxZ, const double InTargetPixels,
	const uint64 InGenerator, const uint64 InMaterial, FVoxelTerrainStagePlan& OutPlan, FString& OutError,
	const FVoxelCoverageIndex* InPredecessor, const FVoxelTerrainPageCostLimits& InLimits)
{
	const int32 Stage = int32(InRepresentation) - int32(EVoxelTerrainRepresentation::VoxelProxy);
	if (Stage < 0 || Stage >= 3 || !Sources[Stage]) { OutError = TEXT("Missing frozen coarse source delta"); return false; }
	return FVoxelTerrainStagePlanner::Build(InRepresentation, Sources[Stage]->Values, InMaximumLevel, InMaximumSamples,
		InMinZ, InMaxZ, InTargetPixels, InGenerator, InMaterial, OutPlan, OutError, InPredecessor, InLimits, &Stages[Stage]);
}

void FVoxelTerrainDomainPlanner::Reset()
{
	for (auto& Source : Sources) Source.Reset();
	for (auto& Stage : Stages) Stage = {};
}

bool FVoxelTerrainDomainPlanner::HasRetryReady() const
{
	for (const auto& Stage : Stages) if (Stage.IsRetryReady()) return true;
	return false;
}

namespace
{
	bool IntersectsSource(const FVoxelTerrainGeometry& InGeometry, const FVoxelTerrainStageSource& InSource)
	{
		FIntVector Min, Max;
		if (!InGeometry.Key.TryBounds(InGeometry.WorldMinZ, InGeometry.WorldMaxZ, Min, Max)) return false;
		const auto& Center = InSource.Source.Center;
		const double X = Center.X - FMath::Clamp(Center.X, Min.X, Max.X);
		const double Y = Center.Y - FMath::Clamp(Center.Y, Min.Y, Max.Y);
		return X * X + Y * Y <= FMath::Square(double(InSource.OuterRadiusCells)) &&
			(InGeometry.Key.IsColumn() || (Max.Z > int64(Center.Z) - InSource.VerticalHalfExtentCells && Min.Z <= int64(Center.Z) + InSource.VerticalHalfExtentCells));
	}

	int32 ResolveActualFirstSideCells(const FVoxelTerrainStageSource& InSource, const TMap<FVoxelTerrainSpatialKey, FVoxelTerrainGeometry>& InPages)
	{
		int32 Result = MAX_int32;
		for (const auto& Pair : InPages)
		{
			if (IntersectsSource(Pair.Value, InSource))
			{
				Result = FMath::Min(Result, static_cast<int32>(Pair.Key.SideCells()));
			}
		}

		return Result == MAX_int32 ? 16 : Result;
	}
}

bool FVoxelTerrainDomainPlanner::Build(TConstArrayView<FVoxelStreamingSource> InSources, const FVoxelWorldManifest& InManifest,
	const FVoxelViewSettings& InSettings, FVoxelInterestSet& InOutInterest, FString& OutError, FVoxelTerrainDomainPlanner* InPersistent)
{
	check(InOutInterest.FinePagePlan);
	InOutInterest.CoarseFailedRootCount = 0;
	const auto Resources = InPersistent ? InPersistent->Stages[0].Resources : nullptr;
	const auto Consumer = InPersistent ? InPersistent->Stages[0].Consumer : FVoxelGenerationConsumerKey{};
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> SourceScratch;
	if (Resources && !Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working,
		FMath::Max<uint64>(1, VoxelBoundedAllocation::ArrayBytes<FVoxelTerrainStageSource>(InSources.Num())), SourceScratch))
	{ OutError = TEXT("CapacityBlocked: coarse Source scratch"); return false; }

	TMap<FVoxelTerrainSpatialKey, FVoxelTerrainGeometry> Geometry;
	TSet<FVoxelTerrainSpatialKey> ResolutionFloor;
	InOutInterest.VoxelProxy.Reset();
	InOutInterest.Surface.Reset();
	InOutInterest.Macro.Reset();
	const auto ReserveOutput = [&](const int64 InGeometryRows, const int64 InFloorRows,
		const int64 InProxyRows, const int64 InSurfaceRows, const int64 InMacroRows)
	{
		if (InGeometryRows > MAX_int32 || InFloorRows > MAX_int32 || InProxyRows > MAX_int32 ||
			InSurfaceRows > MAX_int32 || InMacroRows > MAX_int32)
		{ OutError = TEXT("Oversized: coarse composed output rows"); return false; }
		const uint64 OldBytes = Geometry.GetAllocatedSize() + ResolutionFloor.GetAllocatedSize() +
			InOutInterest.VoxelProxy.GetAllocatedSize() + InOutInterest.Surface.GetAllocatedSize() + InOutInterest.Macro.GetAllocatedSize();
		const uint64 NewBytes = FMath::Max<uint64>(Geometry.GetAllocatedSize(),
			VoxelBoundedAllocation::MapBytes<FVoxelTerrainSpatialKey, FVoxelTerrainGeometry>(int32(InGeometryRows))) +
			FMath::Max<uint64>(ResolutionFloor.GetAllocatedSize(), VoxelBoundedAllocation::SetBytes<FVoxelTerrainSpatialKey>(int32(InFloorRows))) +
			FMath::Max<uint64>(InOutInterest.VoxelProxy.GetAllocatedSize(), VoxelBoundedAllocation::SetBytes<FVoxelViewKey>(int32(InProxyRows))) +
			FMath::Max<uint64>(InOutInterest.Surface.GetAllocatedSize(), VoxelBoundedAllocation::SetBytes<FVoxelSurfaceTileKey>(int32(InSurfaceRows))) +
			FMath::Max<uint64>(InOutInterest.Macro.GetAllocatedSize(), VoxelBoundedAllocation::SetBytes<FVoxelMacroTileKey>(int32(InMacroRows)));
		const uint64 PeakBytes = FMath::Max<uint64>(1, OldBytes + NewBytes);
		if (Resources)
		{
			if (PeakBytes > Resources->Snapshot().Limits.InputBytes)
			{ OutError = TEXT("Oversized: coarse composed allocation peak"); return false; }
			if ((!InOutInterest.CoarseCompositionLease && !Resources->Acquire(Consumer,
				EVoxelTerrainResourcePool::Input, PeakBytes, InOutInterest.CoarseCompositionLease)) ||
				!InOutInterest.CoarseCompositionLease->Resize(PeakBytes))
			{ OutError = TEXT("CapacityBlocked: coarse composed allocation peak"); return false; }
		}
		Geometry.Reserve(int32(InGeometryRows)); ResolutionFloor.Reserve(int32(InFloorRows));
		InOutInterest.VoxelProxy.Reserve(int32(InProxyRows)); InOutInterest.Surface.Reserve(int32(InSurfaceRows));
		InOutInterest.Macro.Reserve(int32(InMacroRows));
		if (Resources)
		{
			const bool bResized = InOutInterest.CoarseCompositionLease->Resize(FMath::Max<uint64>(1, NewBytes));
			check(bResized);
		}
		return true;
	};
	if (!ReserveOutput(InOutInterest.FinePagePlan->Pages.Num(), 0, 0, 0, 0)) return false;
	for (const FVoxelFinePageKey& Page : InOutInterest.FinePagePlan->Pages)
	{
		const FVoxelTerrainSpatialKey Key{EVoxelTerrainRepresentation::Fine, Page.Coordinate, Page.Level};
		Geometry.Add(Key, {Key, 1, InManifest.Settings.MinZ, InManifest.Settings.MaxZ, InManifest.BaseSampleHash, InManifest.RegistryHash});
	}

	FVoxelCoverageIndex PredecessorCoverage;
	if (InPersistent && InPersistent->Stages[0].Resources)
		PredecessorCoverage.BindResources(InPersistent->Stages[0].Resources.ToSharedRef(), InPersistent->Stages[0].Consumer);
	const auto AddCoverage = [&PredecessorCoverage, &OutError](const auto& Pages)
	{
		for (const auto& Pair : Pages)
		{
			FVoxelCoverageRecord Record;
			Record.Geometry = Pair.Value;
			if (!Pair.Key.TryBounds(Pair.Value.WorldMinZ, Pair.Value.WorldMaxZ, Record.Min, Record.Max) || !PredecessorCoverage.Upsert(Record, OutError))
			{
				return false;
			}
		}
		return true;
	};

	for (const auto& Pair : InOutInterest.FineShardDemands)
	{
		const auto& Shard = Pair.Key;
		const FVoxelTerrainSpatialKey Key{EVoxelTerrainRepresentation::Fine, Shard.Min / Shard.SideCells,
			uint8(FMath::FloorLog2(uint32(Shard.SideCells / 16)))};
		FVoxelCoverageRecord Record;
		Record.Geometry = {Key, 1, InManifest.Settings.MinZ, InManifest.Settings.MaxZ,
			InManifest.BaseSampleHash, InManifest.RegistryHash};
		if (!Key.TryBounds(InManifest.Settings.MinZ, InManifest.Settings.MaxZ, Record.Min, Record.Max) ||
			!PredecessorCoverage.Upsert(Record, OutError)) return false;
	}

	for (const EVoxelTerrainRepresentation Representation :
		{EVoxelTerrainRepresentation::VoxelProxy, EVoxelTerrainRepresentation::Surface, EVoxelTerrainRepresentation::Macro})
	{
		TArray<FVoxelTerrainStageSource> Sources;
		Sources.Reserve(InSources.Num());

		for (const FVoxelStreamingSource& InputSource : InSources)
		{
			FVoxelStreamingSource Source = InputSource;
			const int32 StageIndex = static_cast<int32>(Representation) - 2;

			if (const auto* Accepted = InOutInterest.Sources.Find(Source.Id); Accepted && Accepted->bHasStageCenters)
			{
				Source.Center = Accepted->StageCenters[StageIndex];
			}

			const bool bLocal = Source.Has(EVoxelStreamingCapability::LocalRefinement) && !Source.Has(EVoxelStreamingCapability::WorldVisual);

			if (!Source.Has(EVoxelStreamingCapability::WorldVisual) && !(bLocal && Representation == EVoxelTerrainRepresentation::VoxelProxy))
			{
				continue;
			}

			const FVoxelResolvedViewRanges Ranges = VoxelViewLod::ResolveViewRanges(Source);

			if ((Representation == EVoxelTerrainRepresentation::VoxelProxy && !bLocal && Source.View.VoxelProxyRadiusCells <= 0) ||
				(Representation == EVoxelTerrainRepresentation::Surface && Source.View.SurfaceRadiusCells <= 0) ||
				(Representation == EVoxelTerrainRepresentation::Macro && Source.View.MacroRadiusCells <= 0))
			{
				continue;
			}

			const int32 Outer = Representation == EVoxelTerrainRepresentation::VoxelProxy
									? (bLocal ? Ranges.Fine + Source.View.InitialChunkSideCells * 2 : Ranges.Proxy)
								: Representation == EVoxelTerrainRepresentation::Surface ? Ranges.Surface
																						 : Ranges.Macro;

			const int32 PreviousOuter = Source.View.SurfaceRadiusCells > 0		? Ranges.Surface
										: Source.View.VoxelProxyRadiusCells > 0 ? Ranges.Proxy
																				: Ranges.Fine;

			const int32 Inner = Representation == EVoxelTerrainRepresentation::Macro ? PreviousOuter : 0;

			if (Outer <= Inner || Outer <= 0)
			{
				continue;
			}

			Sources.Add({Source, Outer,
				Representation == EVoxelTerrainRepresentation::VoxelProxy ? Source.View.VoxelProxyVerticalHalfExtentCells : (MAX_int32 / 2), Inner});
		}

		if (Sources.IsEmpty())
		{
			if (InPersistent && !InPersistent->ApplySourceDelta(Representation, Sources, OutError)) return false;
			continue;
		}

		const uint8 Maximum = Representation == EVoxelTerrainRepresentation::VoxelProxy ? InSettings.Lod.MaximumVoxelProxyLevel
							  : Representation == EVoxelTerrainRepresentation::Surface	? InSettings.Lod.MaximumSurfaceSpatialLevel
																						: InSettings.Lod.MaximumMacroSpatialLevel;

		const int32 MaximumSamplesPerAxis = Representation == EVoxelTerrainRepresentation::VoxelProxy ? InSettings.Lod.MaximumVoxelProxySamplesPerAxis
																									  : InSettings.Lod.MaximumHeightfieldSamplesPerAxis;

		FVoxelTerrainStagePlan Plan;
		const FVoxelTerrainPageCostLimits Limits{uint64(InSettings.Lod.MaximumPageEstimatedTriangles),
			uint64(InSettings.Lod.MaximumPageEstimatedMeshMegabytes) * 1024 * 1024, InSettings.Lod.MaximumPageEstimatedBuildMilliseconds};
		if (InPersistent)
		{
			if (!InPersistent->ApplySourceDelta(Representation, Sources, OutError) ||
				!InPersistent->AdvanceDirtyRoots(Representation, Maximum, MaximumSamplesPerAxis, InManifest.Settings.MinZ, InManifest.Settings.MaxZ,
					InSettings.Lod.TargetScreenErrorPixels, InManifest.BaseSampleHash, InManifest.RegistryHash, Plan, OutError, &PredecessorCoverage, Limits)) return false;
		}
		else if (!FVoxelTerrainStagePlanner::Build(Representation, Sources, Maximum, MaximumSamplesPerAxis, InManifest.Settings.MinZ, InManifest.Settings.MaxZ,
				InSettings.Lod.TargetScreenErrorPixels, InManifest.BaseSampleHash, InManifest.RegistryHash, Plan, OutError, &PredecessorCoverage,
				Limits))
		{
			return false;
		}

		for (const FVoxelTerrainStageSource& StageSource : Sources)
		{
			InOutInterest.Sources.FindOrAdd(StageSource.Source.Id).FirstSideCells[static_cast<int32>(Representation) - 2] =
				ResolveActualFirstSideCells(StageSource, Plan.Pages);
		}
		if (Plan.ResourceLease && !Plan.ResourceLease->Transfer(EVoxelTerrainResourcePool::Input))
		{
			const auto Snapshot = InPersistent ? InPersistent->Stages[int32(Representation) - int32(EVoxelTerrainRepresentation::VoxelProxy)].Resources->Snapshot() : FVoxelTerrainResourceSnapshot{};
			OutError = Plan.ResourceLease->Bytes() > Snapshot.Limits.InputBytes ? TEXT("Oversized: coarse output root") : TEXT("CapacityBlocked: coarse output root");
			if (InPersistent)
			{
				auto& Stage = InPersistent->Stages[int32(Representation) - int32(EVoxelTerrainRepresentation::VoxelProxy)];
				Stage.bCapacityBlocked = true;
				Stage.RetryCapacityVersion = Stage.Resources->Snapshot().ReleaseVersion;
			}
			return false;
		}
		if (!AddCoverage(Plan.Pages))
		{
			return false;
		}

		const int64 NewRows = Plan.Pages.Num();
		if (!ReserveOutput(int64(Geometry.Num()) + NewRows, int64(ResolutionFloor.Num()) + Plan.ResolutionFloorPages.Num(),
			int64(InOutInterest.VoxelProxy.Num()) + (Representation == EVoxelTerrainRepresentation::VoxelProxy ? NewRows : 0),
			int64(InOutInterest.Surface.Num()) + (Representation == EVoxelTerrainRepresentation::Surface ? NewRows : 0),
			int64(InOutInterest.Macro.Num()) + (Representation == EVoxelTerrainRepresentation::Macro ? NewRows : 0))) return false;
		Geometry.Append(Plan.Pages);
		InOutInterest.CoarseFailedRootCount += Plan.FailedRootCount;
		ResolutionFloor.Append(Plan.ResolutionFloorPages);

		for (const auto& Pair : Plan.Pages)
		{
			const FVoxelTerrainSpatialKey& Key = Pair.Key;
			if (Representation == EVoxelTerrainRepresentation::VoxelProxy)
			{
				InOutInterest.VoxelProxy.Add({Key.Coordinate, Key.Level});
			}
			else if (Representation == EVoxelTerrainRepresentation::Surface)
			{
				InOutInterest.Surface.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
			}
			else
			{
				InOutInterest.Macro.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
			}
		}
	}

	InOutInterest.Geometry = MoveTemp(Geometry);
	InOutInterest.ResolutionFloorPages = MoveTemp(ResolutionFloor);
	return true;
}
