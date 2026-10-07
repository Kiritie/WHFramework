#pragma once

#include "CoreMinimal.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Generation/VoxelWorldManifest.h"
#include "Voxel/Streaming/VoxelTerrainDomainPlanner.h"

class FVoxelTerrainResourceLease;

struct WHFRAMEWORK_API FVoxelInterestDelta
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	TSet<FIntVector> AddedExact;
	TSet<FIntVector> RemovedExact;
	TSet<FIntVector> AddedSimulation;
	TSet<FIntVector> RemovedSimulation;
	TSet<FIntVector> AddedFine;
	TSet<FIntVector> RemovedFine;
	TVoxelFineSetDelta<FVoxelFineShardKey> FineResidency;
	TSet<FVoxelViewKey> AddedProxy;
	TSet<FVoxelViewKey> RemovedProxy;
	TSet<FVoxelSurfaceTileKey> AddedSurface;
	TSet<FVoxelSurfaceTileKey> RemovedSurface;
	TSet<FVoxelMacroTileKey> AddedMacro;
	TSet<FVoxelMacroTileKey> RemovedMacro;
	TArray<FVoxelTerrainSpatialKey> ChangedGeometry;
	bool bVisualPartitionChanged = false;
	bool bRuntimeDemandChanged = false;
	bool bFineResidencyChanged = false;
	bool bPageLayoutChanged = false;

	void Reset()
	{
		*this = {};
	}
};

enum class EVoxelSourceChange : uint8
{
	None = 0,
	Priority = 1 << 0,
	RuntimeDemand = 1 << 1,
	FineResidency = 1 << 2,
	PageLayout = 1 << 3,
	CoarseTopology = 1 << 4
};
ENUM_CLASS_FLAGS(EVoxelSourceChange)

// GT冻结实际回执；Worker只消费该证据，不能从Wanted或Prepared推导安全覆盖。
struct WHFRAMEWORK_API FVoxelFineResidencyFeedback
{
	uint64 WorldEpoch = 0;
	uint64 CoverageRootSerial = 0;
	uint64 ContentRevision = 0;
	double PreparationSeconds = 0.5;
	TMap<FGuid, FIntVector> LastAcceptedPositions;
	TMap<FIntVector, FVoxelFineSectionCoverage> Sections;
	TMap<FVoxelFineShardKey, FVoxelFineShardReadiness> Shards;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
};

class WHFRAMEWORK_API FVoxelInterestRuntime
{
public:
	static EVoxelSourceChange ClassifySourceChange(const FVoxelStreamingSource& InPrevious,
		const FVoxelStreamingSource& InCurrent, const FVoxelViewSettings& InSettings,
		const FVoxelSourceInterest* InAccepted = nullptr);
	static bool NeedsUpdate(const FVoxelStreamingSource& InPrevious,
		const FVoxelStreamingSource& InCurrent, const FVoxelViewSettings& InSettings = FVoxelViewSettings(),
		const FVoxelSourceInterest* InAccepted = nullptr);

	bool Update(TConstArrayView<FVoxelStreamingSource> InSources,
		const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
		FVoxelInterestSet& OutInterest, FVoxelInterestDelta& OutDelta,
		bool& bOutSnapshot, const FVoxelFineResidencyFeedback* InFeedback = nullptr);
	bool UpdateSnapshot(TConstArrayView<FVoxelStreamingSource> InSources,
		const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
		TSharedPtr<FVoxelInterestSet, ESPMode::ThreadSafe>& OutInterest, FVoxelInterestDelta& OutDelta,
		bool& bOutSnapshot, const FVoxelFineResidencyFeedback* InFeedback = nullptr);

	void Reset();
	bool HasPlanningRetry() const { return CoarsePlanner.HasRetryReady(); }
	const FString& GetPlanningError() const { return LastPlanningError; }
	void BindPlanningResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer, const double InFirstQueuedSeconds = 0.0)
	{
		PlanningResources = InResources;
		PlanningConsumer = InConsumer;
		PlanningFirstQueuedSeconds = InFirstQueuedSeconds;
		CoarsePlanner.BindResources(InResources, InConsumer);
	}

private:
	struct FSourceState
	{
		FVoxelStreamingSource Source;
		FIntVector ExactAnchor = FIntVector(MAX_int32);
		FIntVector FineAnchor = FIntVector(MAX_int32);
		FIntVector ProxyAnchor = FIntVector(MAX_int32);
		FIntPoint SurfaceAnchor = FIntPoint(MAX_int32);
		FIntPoint MacroAnchor = FIntPoint(MAX_int32);
		FIntVector ProxyPlanCenter = FIntVector::ZeroValue;
		FIntVector SurfacePlanCenter = FIntVector::ZeroValue;
		FIntVector MacroPlanCenter = FIntVector::ZeroValue;
		FIntVector PredictionAnchor = FIntVector(MAX_int32);
		TSharedPtr<const FVoxelInterestSet, ESPMode::ThreadSafe> Exact;
		FVoxelFineResidency FineResidency;
		bool bHasFineResidency = false;
		bool bRequiresTravelBarrier = false;
	};
	struct FSourceRoot
	{
		TMap<FGuid, TSharedPtr<const FSourceState, ESPMode::ThreadSafe>> Values;
	};

	static FIntVector Snap3D(const FIntVector& InCell, int32 InStep);
	static FIntPoint Snap2D(const FIntVector& InCell, int32 InStep);
	TSharedPtr<const FSourceRoot, ESPMode::ThreadSafe> SourceRoot;
	TSharedPtr<const FVoxelFineResidencyLeases, ESPMode::ThreadSafe> FineLeases;
	TSharedPtr<const FVoxelFinePageLayout, ESPMode::ThreadSafe> FineLayout;
	FVoxelTerrainDomainPlanner CoarsePlanner;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> PlanningResources;
	FVoxelGenerationConsumerKey PlanningConsumer;
	double PlanningFirstQueuedSeconds = 0.0;
	FString LastPlanningError;
	TSharedPtr<const FVoxelInterestSet, ESPMode::ThreadSafe> LatestInterest;
	FVoxelViewSettings LatestSettings;
	bool bHasSettings = false;
	uint64 PublicationSerial = 0;
	uint64 DemandSerial = 0;
	uint64 PrioritySerial = 0;
	uint64 ResidencySerial = 0;
	uint64 LayoutSerial = 0;
	uint64 FineDemandSequence = 0;
};
