#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelViewTypes.h"
#include "Voxel/Rendering/VoxelTerrainViewPlan.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"

struct WHFRAMEWORK_API FVoxelSourceInterest
{
	FVoxelStreamingSource Source;
	TSet<FIntVector> DataSections;
	TSet<FIntVector> FineDataSections;
	TSet<FIntVector> CollisionSections;
};

struct WHFRAMEWORK_API FVoxelExactDemand
{
	bool bData = false;
	bool bExact = false;
	bool bCollision = false;
	bool bSimulation = false;
	bool bFineRender = false;
	bool bFineData = false;
	bool bWarmupData = false;
	bool bWarmupCollision = false;
	bool bMovementCriticalCollision = false;
	int32 Priority = MAX_int32;
	double DistanceCells = MAX_dbl;
	double HorizontalDistanceCells = MAX_dbl;
	double ForwardScore = 0.0;
};

struct WHFRAMEWORK_API FVoxelInterestSet
{
	TMap<FGuid, FVoxelSourceInterest> Sources;
	FVoxelTerrainViewPlan TerrainPlan;
	TSharedPtr<const TSet<FIntVector>, ESPMode::ThreadSafe> FineSections;
	TArray<FVoxelViewAdmission> Admissions;
	TArray<int32> AdmissionLanes[4];
	TMap<FIntVector, FVoxelExactDemand> Exact;
	TArray<FIntVector> ExactOrder;
	TArray<FIntVector> PlayableFineKeys;
	TMap<FIntVector, FVoxelExactDemand> Warmup;
	TSet<FVoxelViewKey> VoxelProxy;
	TSet<FVoxelSurfaceTileKey> Surface;
	TSet<FVoxelMacroTileKey> Macro;
	TMap<FVoxelViewKey, int32> VoxelProxyPriorities;
	TMap<FVoxelSurfaceTileKey, int32> SurfacePriorities;
	TMap<FVoxelMacroTileKey, int32> MacroPriorities;

	uint64 GetAllocatedBytes() const
	{
		uint64 SourceBytes = Sources.GetAllocatedSize();
		for (const auto& Pair : Sources)
		{
			SourceBytes += Pair.Value.DataSections.GetAllocatedSize() + Pair.Value.FineDataSections.GetAllocatedSize() + Pair.Value.CollisionSections.GetAllocatedSize();
		}
		return SourceBytes + (FineSections ? FineSections->GetAllocatedSize() : 0) + Warmup.GetAllocatedSize() + ExactOrder.GetAllocatedSize() + PlayableFineKeys.GetAllocatedSize() + Admissions.GetAllocatedSize() + AdmissionLanes[0].GetAllocatedSize() + AdmissionLanes[1].GetAllocatedSize() +
			AdmissionLanes[2].GetAllocatedSize() + AdmissionLanes[3].GetAllocatedSize() + Exact.GetAllocatedSize() + VoxelProxy.GetAllocatedSize() + Surface.GetAllocatedSize() + Macro.GetAllocatedSize() +
			VoxelProxyPriorities.GetAllocatedSize() + SurfacePriorities.GetAllocatedSize() + MacroPriorities.GetAllocatedSize() +
			TerrainPlan.Roots.GetAllocatedSize() + TerrainPlan.Leaves.GetAllocatedSize() +
			TerrainPlan.Required.GetAllocatedSize() + TerrainPlan.FineDependencies.GetAllocatedSize();
	}
};

struct WHFRAMEWORK_API FVoxelViewSettings
{
	FVoxelStreamingSourceView DefaultSourceView;
	float PlayableFineRadiusFraction = 0.75f;


	int32 MacroRadiusCells = 16000;




	uint8 MaximumMacroLevel = 4;

	int32 MaximumSurfaceTiles = 256;

	int32 MaximumMacroTiles = 128;
	double MaximumTextureStretchCells = 4.0;
	int32 VoxelProxyTileSide = 16;
	int32 SurfaceTileSide = 32;
	int32 MacroTileSide = 2048;
	int32 MaximumTerrainLeaves = 8192;
	int32 FineBuildsPerFrame = 24;
	int32 VoxelProxyBuildsPerFrame = 12;
	int32 SurfaceBuildsPerFrame = 8;
	int32 MacroBuildsPerFrame = 8;
	int32 DataBuildsPerFrame = 128;
	int32 CompletedResultsPerFrame = 32;
	int32 HeavyResultsPerFrame = 8;
	float BuildAdmissionMilliseconds = 2.0f;
};
