#pragma once

#include "CoreMinimal.h"
#include "Voxel/Authoring/VoxelViewProfile.h"
#include "Voxel/Rendering/VoxelViewTypes.h"
#include "Voxel/Rendering/VoxelFinePage.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"
#include "Voxel/Rendering/VoxelTerrainViewPlan.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"

struct WHFRAMEWORK_API FVoxelSourceInterest
{
	FVoxelStreamingSource Source;
	int32 FirstSideCells[3] = {};
	FIntVector StageCenters[3];
	bool bHasStageCenters = false;
	TSet<FIntVector> DataSections;
	TSet<FIntVector> FineDataSections;
	FVoxelGenerationBounds FineBounds;
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
	bool bMovementCriticalFine = false;
	bool bMovementCriticalData = false;
	int32 Priority = MAX_int32;
	double DistanceCells = MAX_dbl;
	double HorizontalDistanceCells = MAX_dbl;
	double ForwardScore = 0.0;
};

// Interest 任务生成固定发布输入，供 GT 游标分步消费。
struct WHFRAMEWORK_API FVoxelTerrainPublicationInputs
{
	TArray<FVoxelTerrainGeometry> Geometry;
	TArray<FVoxelViewKey> Roots;
	TArray<FIntVector> Fine;
	TArray<FVoxelViewKey> Proxy;
	TArray<FVoxelSurfaceTileKey> Surface;
	TArray<FVoxelMacroTileKey> Macro;
	TArray<FVoxelFinePageKey> FinePages;
	TArray<TPair<FIntVector, FVoxelFinePageKey>> FineOwners;
	TArray<FVoxelViewAdmission> Admissions;
	TArray<int32> AdmissionPriorities;
	TArray<int32> AdmissionLanes[4];
	uint64 GetAllocatedBytes() const
	{
		return AdmissionLanes[0].GetAllocatedSize() + AdmissionLanes[1].GetAllocatedSize() + AdmissionLanes[2].GetAllocatedSize() +
			   AdmissionLanes[3].GetAllocatedSize() + Geometry.GetAllocatedSize() + Roots.GetAllocatedSize() + Fine.GetAllocatedSize() +
			   Proxy.GetAllocatedSize() + Surface.GetAllocatedSize() + Macro.GetAllocatedSize() + FinePages.GetAllocatedSize() + FineOwners.GetAllocatedSize() +
			   Admissions.GetAllocatedSize() + AdmissionPriorities.GetAllocatedSize();
	}
};

struct WHFRAMEWORK_API FVoxelInterestSet
{
	uint64 PublicationRevision = 0;
	uint64 DemandRevision = 0;
	uint64 PriorityRevision = 0;
	TMap<FGuid, FVoxelSourceInterest> Sources;
	FVoxelTerrainViewPlan TerrainPlan;
	TSharedPtr<const FVoxelFinePagePlan, ESPMode::ThreadSafe> FinePagePlan;
	TSharedPtr<const FVoxelTerrainPublicationInputs, ESPMode::ThreadSafe> PublicationInputs;
	TMap<FVoxelTerrainSpatialKey, FVoxelTerrainGeometry> Geometry;
	TSet<FVoxelTerrainSpatialKey> ResolutionFloorPages;
	TSet<FVoxelTerrainSpatialKey> CriticalDependencies;
	FString PlanningError;
	TSharedPtr<const TSet<FIntVector>, ESPMode::ThreadSafe> FineSections;
	TArray<FVoxelViewAdmission> Admissions;
	TArray<int32> AdmissionPriorities;
	TArray<int32> AdmissionLanes[4];
	TMap<FIntVector, FVoxelExactDemand> Exact;
	TMap<FVoxelFinePageKey, int32> FinePagePriorities;
	TSet<FVoxelFinePageKey> CriticalFinePages;
	TArray<FIntVector> ExactOrder;
	TArray<int32> ExactAdmissionLanes[2];
	TArray<FIntVector> PlayableFineKeys;
	TMap<FIntVector, FVoxelExactDemand> Warmup;
	TSet<FVoxelViewKey> VoxelProxy;
	TSet<FVoxelSurfaceTileKey> Surface;
	TSet<FVoxelMacroTileKey> Macro;
	TMap<FVoxelViewKey, int32> VoxelProxyPriorities;
	TMap<FVoxelSurfaceTileKey, int32> SurfacePriorities;
	TMap<FVoxelMacroTileKey, int32> MacroPriorities;

	const FVoxelTerrainGeometry* FindGeometry(const FVoxelViewKey& InKey) const;
	const FVoxelTerrainGeometry* FindGeometry(const FVoxelSurfaceTileKey& InKey) const;
	const FVoxelTerrainGeometry* FindGeometry(const FVoxelMacroTileKey& InKey) const;

	uint64 GetAllocatedBytes() const
	{
		uint64 SourceBytes = Sources.GetAllocatedSize();
		for (const auto& Pair : Sources)
		{
			SourceBytes += Pair.Value.DataSections.GetAllocatedSize() + Pair.Value.FineDataSections.GetAllocatedSize() + Pair.Value.CollisionSections.GetAllocatedSize();
		}
		return FinePagePriorities.GetAllocatedSize() + CriticalFinePages.GetAllocatedSize() + AdmissionPriorities.GetAllocatedSize() +
			   (PublicationInputs ? PublicationInputs->GetAllocatedBytes() : 0) + Geometry.GetAllocatedSize() + ResolutionFloorPages.GetAllocatedSize() +
			   CriticalDependencies.GetAllocatedSize() +
			   (FinePagePlan ? FinePagePlan->Pages.GetAllocatedSize() + FinePagePlan->SectionOwners.GetAllocatedSize() : 0) + SourceBytes +
			   (FineSections ? FineSections->GetAllocatedSize() : 0) + Warmup.GetAllocatedSize() + ExactOrder.GetAllocatedSize() +
			   ExactAdmissionLanes[0].GetAllocatedSize() + ExactAdmissionLanes[1].GetAllocatedSize() + PlayableFineKeys.GetAllocatedSize() +
			   Admissions.GetAllocatedSize() + AdmissionLanes[0].GetAllocatedSize() + AdmissionLanes[1].GetAllocatedSize() +
			   AdmissionLanes[2].GetAllocatedSize() + AdmissionLanes[3].GetAllocatedSize() + Exact.GetAllocatedSize() + VoxelProxy.GetAllocatedSize() +
			   Surface.GetAllocatedSize() + Macro.GetAllocatedSize() + VoxelProxyPriorities.GetAllocatedSize() + SurfacePriorities.GetAllocatedSize() +
			   MacroPriorities.GetAllocatedSize() + TerrainPlan.Roots.GetAllocatedSize() + TerrainPlan.Leaves.GetAllocatedSize() +
			   TerrainPlan.Required.GetAllocatedSize() + TerrainPlan.FineDependencies.GetAllocatedSize();
	}
};

struct WHFRAMEWORK_API FVoxelViewSettings
{
	FVoxelStreamingSourceView DefaultSourceView;
	FVoxelLodTopologySettings Lod;
	FVoxelStreamingReadinessPolicy Readiness;
	FVoxelStreamingPlanningPolicy Planning;
	FVoxelStreamingPerformanceBudget Budget;
	double MaximumTextureStretchCells = 8.0;
};
