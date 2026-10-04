#include "Voxel/Streaming/VoxelFineDomain.h"

#include "Voxel/Rendering/VoxelFinePage.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Generation/VoxelWorldManifest.h"

bool VoxelFineDomain::CompleteInitialFamilies(TConstArrayView<FVoxelStreamingSource> InSources,
	const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
	FVoxelInterestSet& InOutInterest, FString& OutError)
{
	if (InManifest.Settings.MinZ >= InManifest.Settings.MaxZ || InManifest.Settings.MinZ % 16 != 0 ||
		InManifest.Settings.MaxZ % 16 != 0)
	{
		OutError = TEXT("The finite world must have valid 16-cell-aligned vertical bounds");
		return false;
	}
	for (const FVoxelStreamingSource& Source : InSources)
	{
		if (Source.Has(EVoxelStreamingCapability::FineVisual) &&
			!VoxelStreamingGeometry::ValidateInitialSide(Source.View.InitialChunkSideCells,
				InSettings.Lod.MaximumFineLevel, OutError))
		{
			return false;
		}
	}
	TArray<FIntVector> FamilySections;
	for (const FVoxelStreamingSource& Source : InSources)
	{
		if (!Source.Has(EVoxelStreamingCapability::FineVisual))
		{
			continue;
		}
		FVoxelSourceInterest* SourceInterest = InOutInterest.Sources.Find(Source.Id);
		if (!SourceInterest)
		{
			continue;
		}
		const uint8 InitialLevel = static_cast<uint8>(FMath::FloorLog2(uint32(Source.View.InitialChunkSideCells / 16)));
		const int32 SectionsPerSide = 1 << InitialLevel;
		TSet<FVoxelFinePageKey> InitialPages;
		for (const FIntVector& Section : SourceInterest->FineDataSections)
		{
			const FVoxelExactDemand* Existing = InOutInterest.Exact.Find(Section);
			const FVector Min(Section * 16);
			const FVector Max = Min + FVector(16);
			const double NearX = FMath::Clamp(double(Source.Center.X), Min.X, Max.X);
			const double NearY = FMath::Clamp(double(Source.Center.Y), Min.Y, Max.Y);
			if (!Existing || !Existing->bFineRender || Source.View.FineRadiusCells <= 0 ||
				Max.Z < double(Source.Center.Z) - Source.View.FineVerticalHalfExtentCells ||
				Min.Z > double(Source.Center.Z) + Source.View.FineVerticalHalfExtentCells ||
				FVector2D::Distance(FVector2D(Source.Center.X, Source.Center.Y), FVector2D(NearX, NearY)) > Source.View.FineRadiusCells)
			{
				continue;
			}
			InitialPages.Add({FIntVector(static_cast<int32>(VoxelStreamingGeometry::FloorDivide(Section.X, SectionsPerSide)),
				static_cast<int32>(VoxelStreamingGeometry::FloorDivide(Section.Y, SectionsPerSide)),
				static_cast<int32>(VoxelStreamingGeometry::FloorDivide(Section.Z, SectionsPerSide))), InitialLevel});
		}
		for (const FVoxelFinePageKey& Page : InitialPages)
		{
			FVoxelTerrainSpatialKey Spatial{EVoxelTerrainRepresentation::Fine, Page.Coordinate, Page.Level};
			FIntVector Min;
			FIntVector Max;
			if (!Spatial.TryBounds(InManifest.Settings.MinZ, InManifest.Settings.MaxZ, Min, Max))
			{
				OutError = TEXT("An initial Fine family overflows cell coordinates");
				return false;
			}
			FVoxelFinePagePlanner::GatherSections(Page, FamilySections);
			for (const FIntVector& Section : FamilySections)
			{
				const int64 MinZ = int64(Section.Z) * 16;
				if (MinZ < InManifest.Settings.MinZ || MinZ + 16 > InManifest.Settings.MaxZ)
				{
					continue;
				}
				SourceInterest->FineDataSections.Add(Section);
				FVoxelExactDemand& Demand = InOutInterest.Exact.FindOrAdd(Section);
				Demand.bData = true;
				Demand.bFineData = true;
				Demand.bFineRender = true;
				Demand.Priority = FMath::Min(Demand.Priority, Source.GetSchedulingPriority());
				const FVector Delta = FVector(Section * 16 + FIntVector(8)) - FVector(Source.Center);
				Demand.DistanceCells = FMath::Min(Demand.DistanceCells, Delta.Size());
				Demand.HorizontalDistanceCells = FMath::Min(Demand.HorizontalDistanceCells, FVector2D(Delta.X, Delta.Y).Size());
			}
		}
	}
	OutError.Reset();
	return true;
}
