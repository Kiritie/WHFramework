#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelViewTypes.h"
#include "Voxel/Rendering/VoxelTerrainViewPlan.h"

struct WHFRAMEWORK_API FVoxelExactDemand
{
	bool bData = false;
	bool bExact = false;
	bool bCollision = false;
	bool bSimulation = false;
	bool bFineRender = false;
	bool bWarmupData = false;
	bool bWarmupCollision = false;
	bool bMovementCriticalCollision = false;
	double DistanceCells = MAX_dbl;
	double ForwardScore = 0.0;
};

struct WHFRAMEWORK_API FVoxelInterestSet
{
	FVoxelTerrainViewPlan TerrainPlan;
	TSharedPtr<const TSet<FIntVector>, ESPMode::ThreadSafe> FineSections;
	TArray<FVoxelViewAdmission> Admissions;
	TArray<int32> AdmissionLanes[4];
	TMap<FIntVector, FVoxelExactDemand> Exact;
	TArray<FIntVector> ExactOrder;
	TMap<FIntVector, FVoxelExactDemand> Warmup;
	TSet<FVoxelViewKey> VoxelProxy;
	TSet<FVoxelSurfaceTileKey> Surface;
	TSet<FVoxelMacroTileKey> Macro;

	uint64 GetAllocatedBytes() const
	{
		return (FineSections ? FineSections->GetAllocatedSize() : 0) + Warmup.GetAllocatedSize() + ExactOrder.GetAllocatedSize() + Admissions.GetAllocatedSize() + AdmissionLanes[0].GetAllocatedSize() + AdmissionLanes[1].GetAllocatedSize() +
			AdmissionLanes[2].GetAllocatedSize() + AdmissionLanes[3].GetAllocatedSize() + Exact.GetAllocatedSize() + VoxelProxy.GetAllocatedSize() + Surface.GetAllocatedSize() + Macro.GetAllocatedSize() +
			TerrainPlan.Roots.GetAllocatedSize() + TerrainPlan.Leaves.GetAllocatedSize() +
			TerrainPlan.Required.GetAllocatedSize() + TerrainPlan.FineDependencies.GetAllocatedSize();
	}
};

struct WHFRAMEWORK_API FVoxelViewSettings
{
	int32 WarmupDataRadius = 24;
	int32 WarmupCollisionRadius = 24;
	int32 FineRadius = 320;
	int32 FinePreload = 32;
	double MaximumTextureStretchCells = 4.0;
	int32 VoxelProxyRadius = 3200;
	int32 SurfaceRadius = 32000;
	int32 MacroRadius = 120000;
	int32 VoxelProxyTileSide = 16;
	int32 SurfaceTileSide = 32;
	int32 MacroTileSide = 2048;
	float TargetScreenErrorPixels = 2.0f;
	uint8 MaximumVoxelProxyLevel = 4;
	uint8 MaximumSurfaceLevel = 4;
	uint8 MaximumMacroLevel = 4;
	int32 MaximumSurfaceTilesPerSource = 256;
	int32 MaximumMacroTilesPerSource = 128;
	int32 MaximumTerrainLeaves = 8192;
	int32 FineBuildsPerFrame = 8;
	int32 VoxelProxyBuildsPerFrame = 12;
	int32 SurfaceBuildsPerFrame = 16;
	int32 MacroBuildsPerFrame = 24;
	int32 DataBuildsPerFrame = 128;
	int32 CompletedResultsPerFrame = 16;
	int32 HeavyResultsPerFrame = 4;
	float BuildAdmissionMilliseconds = 2.0f;
};
