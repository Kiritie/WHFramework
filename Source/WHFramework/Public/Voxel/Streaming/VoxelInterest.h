#pragma once

#include "CoreMinimal.h"
#include "Voxel/Authoring/VoxelViewProfile.h"
#include "Voxel/Rendering/VoxelViewTypes.h"
#include "Voxel/Rendering/VoxelFinePage.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"
#include "Voxel/Rendering/VoxelTerrainViewPlan.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"
#include "Voxel/Task/VoxelTerrainWorkSelection.h"
#include "Voxel/Streaming/VoxelFineResidency.h"
#include "Voxel/Rendering/VoxelFinePageLayout.h"

class FVoxelTerrainResourceLease;
class FVoxelTerrainWorkReceipt;
struct FVoxelChangeHierarchySnapshot;

struct WHFRAMEWORK_API FVoxelSourceInterest
{
	FVoxelStreamingSource Source;
	int32 FirstSideCells[3] = {};
	FIntVector StageCenters[3];
	bool bHasStageCenters = false;
	TSet<FIntVector> DataSections;
	TSet<FIntVector> FineDataSections;
	TSet<FIntVector> F0CoreSections;
	FVoxelGenerationBounds FineBounds;
	TSet<FIntVector> CollisionSections;
	TSet<FIntVector> SimulationSections;
	TSharedPtr<const FVoxelFineResidency, ESPMode::ThreadSafe> FineResidency;
	bool bRequiresTravelBarrier = false;

	const FVoxelFineWindow& GetCurrentWindow() const
	{
		static const FVoxelFineWindow Empty;
		return FineResidency ? FineResidency->GetCurrent() : Empty;
	}
	const TOptional<FVoxelFineWindow>& GetPendingWindow() const
	{
		static const TOptional<FVoxelFineWindow> Empty;
		return FineResidency ? FineResidency->GetPending() : Empty;
	}
	const TSet<FVoxelFineShardKey>& GetFineShards() const
	{
		static const TSet<FVoxelFineShardKey> Empty;
		return FineResidency ? FineResidency->GetResidentShards() : Empty;
	}
};

struct WHFRAMEWORK_API FVoxelFineShardDemand
{
	FVoxelTerrainServicePriority ServicePriority;
	double FirstQueuedSeconds = 0.0;
	uint64 QueueSequence = 0;
	int32 SourceReferences = 0;
	bool bCriticalDependency = false;
};

struct WHFRAMEWORK_API FVoxelExactDemand
{
	FVoxelTerrainServicePriority ServicePriority;
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
	int32 MaximumFeedbackSections = 0;
	TArray<FVoxelTerrainGeometry> Geometry;
	TArray<FVoxelViewKey> Roots;
	TArray<FIntVector> Fine;
	TArray<FIntVector> RuntimeSections;
	TArray<FVoxelViewKey> Proxy;
	TArray<FVoxelSurfaceTileKey> Surface;
	TArray<FVoxelMacroTileKey> Macro;
	TArray<FVoxelFinePageKey> FinePages;
	TArray<FVoxelFineShardKey> FineShards;
	TArray<TPair<FIntVector, FVoxelFinePageKey>> FineOwners;
	TArray<FVoxelViewAdmission> Admissions;
	TArray<int32> AdmissionPriorities;
	TArray<int32> AdmissionLanes[4];
	uint64 GetAllocatedBytes() const
	{
		return AdmissionLanes[0].GetAllocatedSize() + AdmissionLanes[1].GetAllocatedSize() + AdmissionLanes[2].GetAllocatedSize() +
			   AdmissionLanes[3].GetAllocatedSize() + Geometry.GetAllocatedSize() + Roots.GetAllocatedSize() + Fine.GetAllocatedSize() +
			   Proxy.GetAllocatedSize() + Surface.GetAllocatedSize() + Macro.GetAllocatedSize() + FinePages.GetAllocatedSize() + FineOwners.GetAllocatedSize() +
			   Admissions.GetAllocatedSize() + AdmissionPriorities.GetAllocatedSize() + FineShards.GetAllocatedSize() + RuntimeSections.GetAllocatedSize();
	}
};

struct WHFRAMEWORK_API FVoxelInterestSet
{
	uint64 PublicationRevision = 0;
	uint64 DemandRevision = 0;
	uint64 PriorityRevision = 0;
	uint64 ResidencyRevision = 0;
	uint64 LayoutRevision = 0;
	uint64 SafetyContentRevision = 0;
	uint64 SafetyWorldEpoch = 0;
	uint64 SafetyShapeRevision = 0;
	uint64 SafetyRegistryRevision = 0;
	TSharedPtr<const FVoxelChangeHierarchySnapshot, ESPMode::ThreadSafe> SafetyVersions;
	TSharedPtr<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe> PlanningWorkReceipt;
	uint64 RenderCoverageSerial = 0;
	uint64 PhysicsCoverageSerial = 0;
	int32 CoarseFailedRootCount = 0;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CoarsePlanLeases[3];
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> DemandLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CompositionLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CopyLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> FinalizationLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CoarseCompositionLease;
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
	TMap<FVoxelFineShardKey, FVoxelFineShardDemand> FineShardDemands;
	TMap<FVoxelFinePageKey, FVoxelFineLayoutPage> FineShardPages;
	TMap<FVoxelFineShardKey, FVoxelFinePageKey> FineShardOwners;
	TMap<FVoxelFinePageKey, int32> FinePagePriorities;
	TMap<FVoxelFinePageKey, FVoxelTerrainServicePriority> FinePageServicePriorities;
	TMap<FVoxelTerrainSpatialKey, FVoxelTerrainServicePriority> GeometryServicePriorities;
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
	uint64 GetOwnedAllocatedBytes() const;
	uint64 GetFinalizationAllocatedBytes() const;
	uint64 GetResourceAccountedBytes() const;

	// 内联 backing 属于对象或其外层容器，只统计独立分配的容量。
	uint64 GetAllocatedBytes() const;
};

struct WHFRAMEWORK_API FVoxelViewSettings
{
	FVoxelStreamingSourceView DefaultSourceView;
	FVoxelFineStreamingPolicy Fine;
	FVoxelTerrainFrameBudgetSettings FrameBudget;
	FVoxelLodTopologySettings Lod;
	FVoxelStreamingReadinessPolicy Readiness;
	FVoxelStreamingPlanningPolicy Planning;
	FVoxelStreamingPerformanceBudget Budget;
	double MaximumTextureStretchCells = 8.0;
};
