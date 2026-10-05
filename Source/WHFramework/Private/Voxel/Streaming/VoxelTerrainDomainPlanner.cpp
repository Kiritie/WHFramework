#include "Voxel/Streaming/VoxelTerrainDomainPlanner.h"

#include "Voxel/Rendering/VoxelCoverageIndex.h"
#include "Voxel/Rendering/VoxelCoveragePartition.h"
#include "Voxel/Rendering/VoxelTerrainStagePlanner.h"
#include "Voxel/Rendering/VoxelViewLod.h"

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
	const FVoxelViewSettings& InSettings, FVoxelInterestSet& InOutInterest, FString& OutError)
{
	check(InOutInterest.FinePagePlan);

	TMap<FVoxelTerrainSpatialKey, FVoxelTerrainGeometry> Geometry;
	for (const FVoxelFinePageKey& Page : InOutInterest.FinePagePlan->Pages)
	{
		const FVoxelTerrainSpatialKey Key{EVoxelTerrainRepresentation::Fine, Page.Coordinate, Page.Level};
		Geometry.Add(Key, {Key, 1, InManifest.Settings.MinZ, InManifest.Settings.MaxZ, InManifest.BaseSampleHash, InManifest.RegistryHash});
	}

	FVoxelCoverageIndex PredecessorCoverage;
	const auto AddCoverage = [&PredecessorCoverage, &OutError](const auto& Pages)
	{
		for (const auto& Pair : Pages)
		{
			auto Record = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
			Record->Geometry = Pair.Value;
			if (!Pair.Key.TryBounds(Pair.Value.WorldMinZ, Pair.Value.WorldMaxZ, Record->Min, Record->Max) || !PredecessorCoverage.Upsert(Record, OutError))
			{
				return false;
			}
		}
		return true;
	};

	if (!AddCoverage(Geometry))
	{
		return false;
	}

	TSet<FVoxelTerrainSpatialKey> ResolutionFloor;
	InOutInterest.VoxelProxy.Reset();
	InOutInterest.Surface.Reset();
	InOutInterest.Macro.Reset();

	for (const EVoxelTerrainRepresentation Representation :
		{EVoxelTerrainRepresentation::VoxelProxy, EVoxelTerrainRepresentation::Surface, EVoxelTerrainRepresentation::Macro})
	{
		TArray<FVoxelTerrainStageSource> Sources;

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
			continue;
		}

		const uint8 Maximum = Representation == EVoxelTerrainRepresentation::VoxelProxy ? InSettings.Lod.MaximumVoxelProxyLevel
							  : Representation == EVoxelTerrainRepresentation::Surface	? InSettings.Lod.MaximumSurfaceSpatialLevel
																						: InSettings.Lod.MaximumMacroSpatialLevel;

		const int32 MaximumSamplesPerAxis = Representation == EVoxelTerrainRepresentation::VoxelProxy ? InSettings.Lod.MaximumVoxelProxySamplesPerAxis
																									  : InSettings.Lod.MaximumHeightfieldSamplesPerAxis;

		FVoxelTerrainStagePlan Plan;
		if (!FVoxelTerrainStagePlanner::Build(Representation, Sources, Maximum, MaximumSamplesPerAxis, InManifest.Settings.MinZ, InManifest.Settings.MaxZ,
				InSettings.Lod.TargetScreenErrorPixels, InManifest.BaseSampleHash, InManifest.RegistryHash, Plan, OutError, &PredecessorCoverage,
				{uint64(InSettings.Lod.MaximumPageEstimatedTriangles), uint64(InSettings.Lod.MaximumPageEstimatedMeshMegabytes) * 1024 * 1024,
					InSettings.Lod.MaximumPageEstimatedBuildMilliseconds}))
		{
			return false;
		}

		for (const FVoxelTerrainStageSource& StageSource : Sources)
		{
			InOutInterest.Sources.FindOrAdd(StageSource.Source.Id).FirstSideCells[static_cast<int32>(Representation) - 2] =
				ResolveActualFirstSideCells(StageSource, Plan.Pages);
		}

		if (!AddCoverage(Plan.Pages))
		{
			return false;
		}

		Geometry.Append(Plan.Pages);
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
