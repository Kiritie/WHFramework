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

	bool GatherFrontier(const TMap<FVoxelTerrainSpatialKey, FVoxelTerrainGeometry>& InPages,
		TArray<FVoxelTerrainGeometry>& OutBoundary, FString& OutError)
	{
		FVoxelCoverageIndex Index;
		for (const auto& Pair : InPages)
		{
			auto Record = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
			Record->Geometry = Pair.Value;
			if (!Pair.Key.TryBounds(Pair.Value.WorldMinZ, Pair.Value.WorldMaxZ, Record->Min, Record->Max) ||
				!Index.Upsert(Record, OutError)) return false;
		}
		for (const auto& Pair : InPages)
		{
			FIntVector Min, Max;
			Pair.Key.TryBounds(Pair.Value.WorldMinZ, Pair.Value.WorldMaxZ, Min, Max);
			const int32 Axes = Pair.Key.IsColumn() ? 2 : 3;
			for (int32 Axis = 0; Axis < Axes; ++Axis)
			{
				bool bBoundary = false;
				for (const int32 Sign : {-1, 1})
				{
					FVoxelCoverageRegion Required{Min, Max};
					Required.Min[Axis] = Sign < 0 ? Min[Axis] - 1 : Max[Axis];
					Required.Max[Axis] = Required.Min[Axis] + 1;
					if (Axis == 2 && (Required.Max.Z <= Pair.Value.WorldMinZ || Required.Min.Z >= Pair.Value.WorldMaxZ)) continue;
					FVoxelCoverageLocalSnapshot Neighbors;
					Index.Capture(Required.Min, Required.Max, Neighbors);
					TArray<FVoxelCoverageRegion> Cover;
					for (const auto& Record : Neighbors.Records) Cover.Add({Record->Min, Record->Max});
					FVoxelCoverageEvaluationTask Evaluation;
					if (!Evaluation.Begin(MakeArrayView(&Required, 1), Cover, OutError)) return false;
					while (Evaluation.Advance(256) == EVoxelCoverageEvaluation::Pending) {}
					if (Evaluation.GetState() != EVoxelCoverageEvaluation::Covered) { bBoundary = true; break; }
				}
				if (bBoundary) { OutBoundary.Add(Pair.Value); break; }
			}
		}
		return true;
	}

	bool ResolveFirstLevels(TArray<FVoxelTerrainStageSource>& InOutSources,
		TConstArrayView<FVoxelTerrainGeometry> InBoundary, uint8 InMaximumLevel, FString& OutError)
	{
		TArray<int32> Components;
		for (int32 Index = 0; Index < InOutSources.Num(); ++Index) Components.Add(Index);
		const auto Root = [&Components](int32 Index)
		{
			while (Components[Index] != Index) Index = Components[Index];
			return Index;
		};
		for (int32 A = 0; A < InOutSources.Num(); ++A)
			for (int32 B = A + 1; B < InOutSources.Num(); ++B)
			{
				const auto& First = InOutSources[A]; const auto& Second = InOutSources[B];
				const FVector2D Delta(double(First.Source.Center.X) - Second.Source.Center.X, double(First.Source.Center.Y) - Second.Source.Center.Y);
				if (Delta.SizeSquared() <= FMath::Square(double(First.OuterRadiusCells) + Second.OuterRadiusCells) &&
					FMath::Abs(int64(First.Source.Center.Z) - Second.Source.Center.Z) <= int64(First.VerticalHalfExtentCells) + Second.VerticalHalfExtentCells) Components[Root(B)] = Root(A);
			}
		TMap<int32, TArray<FVoxelTerrainGeometry>> Boundaries;
		for (int32 Index = 0; Index < InOutSources.Num(); ++Index)
			for (const auto& Geometry : InBoundary)
				if (IntersectsSource(Geometry, InOutSources[Index])) Boundaries.FindOrAdd(Root(Index)).Add(Geometry);
		for (int32 Index = 0; Index < InOutSources.Num(); ++Index)
		{
			const auto* Boundary = Boundaries.Find(Root(Index));
			FVoxelTerrainStageRange Range;
			if (!Boundary || !VoxelStreamingGeometry::DeriveNextStage(*Boundary, InMaximumLevel, Range, OutError))
			{
				if (!Boundary) OutError = TEXT("Enabled coarse terrain source has no actual predecessor frontier");
				return false;
			}
			InOutSources[Index].FirstLevel = Range.FirstLevel;
		}
		return true;
	}
}

bool FVoxelTerrainDomainPlanner::Build(TConstArrayView<FVoxelStreamingSource> InSources,
	const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
	FVoxelInterestSet& InOutInterest, FString& OutError)
{
	check(InOutInterest.FinePagePlan);
	TMap<FVoxelTerrainSpatialKey, FVoxelTerrainGeometry> Geometry;
	for (const auto& Page : InOutInterest.FinePagePlan->Pages)
	{
		const FVoxelTerrainSpatialKey Key{EVoxelTerrainRepresentation::Fine, Page.Coordinate, Page.Level};
		Geometry.Add(Key, {Key, 1, InManifest.Settings.MinZ, InManifest.Settings.MaxZ, InManifest.BaseSampleHash, InManifest.RegistryHash});
	}
	TArray<FVoxelTerrainGeometry> Boundary;
	if (!GatherFrontier(Geometry, Boundary, OutError)) return false;
	FVoxelCoverageIndex PredecessorCoverage;
	const auto AddCoverage = [&PredecessorCoverage, &OutError](const auto& Pages)
	{
		for (const auto& Pair : Pages)
		{
			auto Record = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
			Record->Geometry = Pair.Value;
			if (!Pair.Key.TryBounds(Pair.Value.WorldMinZ, Pair.Value.WorldMaxZ, Record->Min, Record->Max) ||
				!PredecessorCoverage.Upsert(Record, OutError)) return false;
		}
		return true;
	};
	if (!AddCoverage(Geometry)) return false;
	TSet<FVoxelTerrainSpatialKey> ResolutionFloor;
	InOutInterest.VoxelProxy.Reset(); InOutInterest.Surface.Reset(); InOutInterest.Macro.Reset();
	for (const auto Representation : {EVoxelTerrainRepresentation::VoxelProxy, EVoxelTerrainRepresentation::Surface, EVoxelTerrainRepresentation::Macro})
	{
		TArray<FVoxelTerrainStageSource> Sources;
		for (const auto& InputSource : InSources)

		{
			FVoxelStreamingSource Source = InputSource;
			const int32 StageIndex = static_cast<int32>(Representation) - 2;
			if (const auto* Accepted = InOutInterest.Sources.Find(Source.Id); Accepted && Accepted->bHasStageCenters)
				Source.Center = Accepted->StageCenters[StageIndex];
			const bool bLocal = Source.Has(EVoxelStreamingCapability::LocalRefinement) && !Source.Has(EVoxelStreamingCapability::WorldVisual);
			if (!Source.Has(EVoxelStreamingCapability::WorldVisual) && !(bLocal && Representation == EVoxelTerrainRepresentation::VoxelProxy)) continue;
			const auto Ranges = VoxelViewLod::ResolveViewRanges(Source);
			if ((Representation == EVoxelTerrainRepresentation::VoxelProxy && !bLocal && Source.View.VoxelProxyRadiusCells <= 0) ||
				(Representation == EVoxelTerrainRepresentation::Surface && Source.View.SurfaceRadiusCells <= 0) ||
				(Representation == EVoxelTerrainRepresentation::Macro && Source.View.MacroRadiusCells <= 0)) continue;
			const int32 Outer = Representation == EVoxelTerrainRepresentation::VoxelProxy ? (bLocal ? Ranges.Fine + Source.View.InitialChunkSideCells * 2 : Ranges.Proxy)
				: Representation == EVoxelTerrainRepresentation::Surface ? Ranges.Surface : Ranges.Macro;
			const int32 PreviousOuter = Source.View.SurfaceRadiusCells > 0 ? Ranges.Surface : Source.View.VoxelProxyRadiusCells > 0 ? Ranges.Proxy : Ranges.Fine;
			const int32 Inner = Representation == EVoxelTerrainRepresentation::Macro ? PreviousOuter : 0;
			if (Outer <= Inner || Outer <= 0) continue;
			Sources.Add({Source, Outer, Representation == EVoxelTerrainRepresentation::VoxelProxy ? Source.View.VoxelProxyVerticalHalfExtentCells : (MAX_int32 / 2), 0, Inner});
		}
		if (Sources.IsEmpty()) continue;
		const uint8 Maximum = Representation == EVoxelTerrainRepresentation::VoxelProxy ? InSettings.Lod.MaximumVoxelProxyLevel
			: Representation == EVoxelTerrainRepresentation::Surface ? InSettings.Lod.MaximumSurfaceSpatialLevel : InSettings.Lod.MaximumMacroSpatialLevel;
		if (!ResolveFirstLevels(Sources, Boundary, Maximum, OutError)) return false;
		for (const auto& StageSource : Sources)
			InOutInterest.Sources.FindOrAdd(StageSource.Source.Id).FirstSideCells[static_cast<int32>(Representation) - 2] = 16 << StageSource.FirstLevel;
		FVoxelTerrainStagePlan Plan;
		if (!FVoxelTerrainStagePlanner::Build(Representation, Sources, Maximum, InManifest.Settings.MinZ, InManifest.Settings.MaxZ,
			InSettings.Lod.TargetScreenErrorPixels, InManifest.BaseSampleHash, InManifest.RegistryHash, Plan, OutError, &PredecessorCoverage)) return false;
		if (!Plan.Pages.IsEmpty())
		{
			Boundary.Reset();
			if (!GatherFrontier(Plan.Pages, Boundary, OutError)) return false;
		}
		if (!AddCoverage(Plan.Pages)) return false;
		Geometry.Append(Plan.Pages);
		ResolutionFloor.Append(Plan.ResolutionFloorPages);
		for (const auto& Pair : Plan.Pages)
		{
			const auto& Key = Pair.Key;
			if (Representation == EVoxelTerrainRepresentation::VoxelProxy) InOutInterest.VoxelProxy.Add({Key.Coordinate, Key.Level});
			else if (Representation == EVoxelTerrainRepresentation::Surface) InOutInterest.Surface.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
			else InOutInterest.Macro.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
		}
	}
	InOutInterest.Geometry = MoveTemp(Geometry);
	InOutInterest.ResolutionFloorPages = MoveTemp(ResolutionFloor);
	return true;
}
