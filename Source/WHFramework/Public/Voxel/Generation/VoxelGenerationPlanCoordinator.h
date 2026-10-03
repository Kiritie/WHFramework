#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"
#include "Voxel/Task/VoxelTaskScheduler.h"

enum class EVoxelPlanDependencyState : uint8
{
	Missing,
	Queued,
	Ready,
	Failed
};

struct WHFRAMEWORK_API FVoxelGenerationPlanDependency
{
	EVoxelGenerationPlanKind Kind = EVoxelGenerationPlanKind::Hydrology;
	FVoxelHydrologyRegionKey HydrologyKey;
	FVoxelGenerationTileKey TileKey;
	FVoxelEcologyTileKey EcologyKey;
	uint64 StableHash = 0;

	bool operator==(const FVoxelGenerationPlanDependency& InOther) const;
	FIntVector Coordinate() const;
};

FORCEINLINE uint32 GetTypeHash(const FVoxelGenerationPlanDependency& InDependency)
{
	return HashCombineFast(GetTypeHash(static_cast<uint8>(InDependency.Kind)),
		GetTypeHash(InDependency.Coordinate()));
}

struct WHFRAMEWORK_API FVoxelGenerationDependencyStatus
{
	bool bReady = false;
	bool bFailed = false;
	FString Error;
};

class WHFRAMEWORK_API FVoxelGenerationPlanCoordinator
{
public:
	FVoxelGenerationPlanCoordinator(
		FVoxelTaskScheduler& InScheduler,
		TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> InCache,
		uint64 InWorldEpoch);

	bool GatherForBounds(
		const FVoxelGenerationBounds& InBounds,
		TArray<FVoxelGenerationPlanDependency>& OutDependencies,
		FString& OutError) const;

	TSharedRef<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> RetainDependencies(
		TConstArrayView<FVoxelGenerationPlanDependency> InDependencies,
		const FIntPoint& InCenter) const;

	FVoxelGenerationDependencyStatus Ensure(
		TConstArrayView<FVoxelGenerationPlanDependency> InDependencies,
		EVoxelWorkClass InWorkClass,
		int32 InSourcePriority,
		double InDistanceScore,
		double InForwardScore,
		FIntVector InConsumer = FIntVector(MAX_int32));

	FVoxelGenerationDependencyStatus EnsureEnvironment(const FIntPoint& InCell);
	FVoxelGenerationDependencyStatus EnsureEnvironmentBounds(const FVoxelGenerationBounds& InBounds,
		EVoxelWorkClass InWorkClass, int32 InSourcePriority, double InDistanceScore, double InForwardScore);
	void ReleaseConsumer(const FIntVector& InConsumer);
	uint64 GetWaitingConsumers(EVoxelGenerationPlanKind InKind) const;

	void Reset();

private:
	struct FState
	{
		EVoxelPlanDependencyState State = EVoxelPlanDependencyState::Missing;
		EVoxelWorkClass WorkClass = EVoxelWorkClass::Background;
		int32 SourcePriority = MAX_int32;
		double DistanceScore = MAX_dbl;
		double ForwardScore = 0.0;
		FString Error;
		TSet<FIntVector> Consumers;
	};

	bool IsCached(const FVoxelGenerationPlanDependency& InDependency) const;
	bool Queue(const FVoxelGenerationPlanDependency& InDependency, FState& InOutState);
	bool DonatePriority(FState& InOutState, EVoxelWorkClass InWorkClass,
		int32 InSourcePriority, double InDistanceScore, double InForwardScore);
	void RefreshQueuedPriorities();

	FVoxelTaskScheduler& Scheduler;
	TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
	uint64 WorldEpoch = 0;
	TMap<FVoxelGenerationPlanDependency, FState> States;
};
