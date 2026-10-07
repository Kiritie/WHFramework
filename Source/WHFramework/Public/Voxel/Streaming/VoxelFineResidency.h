#pragma once

#include "CoreMinimal.h"
#include "Voxel/Streaming/VoxelFineSpatial.h"

struct WHFRAMEWORK_API FVoxelFineMovementDelta
{
	TVoxelFineSetDelta<FIntVector> F0;
	TVoxelFineSetDelta<FIntVector> Collision;
	bool bCoreChanged = false;
	bool bGuardChanged = false;
	bool bPriorityChanged = false;
};

struct WHFRAMEWORK_API FVoxelFineSafetyContext
{
	uint64 WorldEpoch = 0;
	uint64 CoverageRootSerial = 0;
	FIntVector LastAcceptedPosition = FIntVector::ZeroValue;
	double PredictionSeconds = 0.0;
};

struct WHFRAMEWORK_API FVoxelFineSectionCoverage
{
	uint64 WorldEpoch = 0;
	uint64 CoverageRootSerial = 0;
	bool bWholeVisualCommitted = false;
	bool bWholeCollisionCommitted = false;
	bool bContentVersionsCurrent = false;
};

struct WHFRAMEWORK_API FVoxelFineSafetyResult
{
	uint64 CoverageRootSerial = 0;
	bool bWholeCoreCovered = false;
	bool bWholeGuardCovered = false;
	bool bWholeSweepCovered = false;
	TSet<FIntVector> MissingSections;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;

	bool IsSafe() const
	{
		return bWholeCoreCovered && bWholeGuardCovered && bWholeSweepCovered;
	}
};

struct WHFRAMEWORK_API FVoxelFineMigrationDecision
{
	bool bStartedPending = false;
	bool bRequiresTravelBarrier = false;
	double RequiredMarginCells = 0.0;
	FVoxelFineSafetyResult Safety;
	TVoxelFineSetDelta<FVoxelFineShardKey> WindowDifference;
	TVoxelFineSetDelta<FVoxelFineShardKey> Residency;
};

struct WHFRAMEWORK_API FVoxelFineShardReadiness
{
	bool bDataCurrent = false;
	bool bGeometryCurrent = false;
	bool bHighResolutionPresented = false;
};

// 每个 Source 的实例由 InterestRuntime 唯一持有；不持有 Blocks、Actor 或第二套调度器。
class WHFRAMEWORK_API FVoxelFineResidency
{
public:
	void BindResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources, const FVoxelGenerationConsumerKey& InConsumer);
	TSharedPtr<const FVoxelFineResidency, ESPMode::ThreadSafe> Capture(FString& OutError) const;
	bool Initialize(const FGuid& InSource, const FVoxelFineWindowSettings& InSettings, const FIntVector& InPosition,
		int32 InCoreRadiusCells, int32 InCoreVerticalCells, int32 InGuardRadiusCells,
		int32 InWorldMinZ, int32 InWorldMaxZ, FString& OutError);
	bool UpdateMovement(const FIntVector& InPosition, const FVector& InVelocityCellsPerSecond, double InPredictionSeconds,
		FVoxelFineMovementDelta& OutDelta, FString& OutError, bool bInProduceDelta = true);
	bool PrepareWindow(const FIntVector& InPosition, const FVector& InVelocityCellsPerSecond,
		double InMeasuredPreparationSeconds, FVoxelFineMigrationDecision& OutDecision, FString& OutError, bool bInProduceDelta = true);
	bool PrepareMigration(const FIntVector& InPosition, const FVector& InVelocityCellsPerSecond,
		double InMeasuredPreparationSeconds, const FVoxelFineSafetyContext& InSafetyContext,
		TFunctionRef<FVoxelFineSectionCoverage(const FIntVector&)> InCommittedCoverage,
		FVoxelFineMigrationDecision& OutDecision, FString& OutError);
	bool EvaluateSafety(const FVoxelFineSafetyContext& InSafetyContext,
		TFunctionRef<FVoxelFineSectionCoverage(const FIntVector&)> InCommittedCoverage,
		FVoxelFineSafetyResult& OutSafety, FString& OutError, bool bInCollectMissing = true) const;
	bool CommitPending(uint64 InExpectedSerial,
		TFunctionRef<FVoxelFineShardReadiness(const FVoxelFineShardKey&)> InReadiness,
		TFunctionRef<bool(const FIntVector&)> InCollisionReady,
		const FVoxelFineSafetyContext& InSafetyContext,
		TFunctionRef<FVoxelFineSectionCoverage(const FIntVector&)> InCommittedCoverage,
		TVoxelFineSetDelta<FVoxelFineShardKey>& OutDelta, FString& OutError, bool bInProduceDelta = true);
	TVoxelFineSetDelta<FVoxelFineShardKey> AbandonPending();
	bool TryAbandonPending(TVoxelFineSetDelta<FVoxelFineShardKey>& OutDelta, FString& OutError, bool bInProduceDelta = true);
	TSet<FVoxelFineShardKey> ResidentShards() const;
	const TSet<FVoxelFineShardKey>& GetResidentShards() const;
	bool Retains(const FVoxelFineShardKey& InKey) const;
	const FVoxelFineWindow& GetCurrent() const;
	const TOptional<FVoxelFineWindow>& GetPending() const;
	const TSet<FIntVector>& GetCore() const;
	const TSet<FIntVector>& GetGuard() const;
	uint64 GetRuntimeDemandRevision() const;
	uint64 GetResidencyRevision() const;
	uint64 GetPriorityRevision() const;
	void Reset();

private:
	bool GatherSafetySweep(const FVoxelFineSafetyContext& InSafetyContext, TSet<FIntVector>& OutSweep, FString& OutError,
		FVoxelFineSpatialCapacity* InCapacity = nullptr) const;

	FGuid Source;
	FVoxelFineWindowSettings Settings;
	TSharedPtr<const FVoxelFineWindow, ESPMode::ThreadSafe> Current;
	TSharedPtr<const TOptional<FVoxelFineWindow>, ESPMode::ThreadSafe> CurrentOptional;
	TSharedPtr<const TOptional<FVoxelFineWindow>, ESPMode::ThreadSafe> Pending;
	TSharedPtr<const TSet<FVoxelFineShardKey>, ESPMode::ThreadSafe> Residents;
	TSharedPtr<const TSet<FIntVector>, ESPMode::ThreadSafe> Core;
	TSharedPtr<const TSet<FIntVector>, ESPMode::ThreadSafe> Guard;
	TOptional<FIntVector> CoreAnchor;
	TOptional<FIntVector> GuardEndAnchor;
	FIntVector LastPosition = FIntVector::ZeroValue;
	FVector LastVelocity = FVector::ZeroVector;
	double LastPredictionSeconds = 0.0;
	int32 CoreRadiusCells = 0;
	int32 CoreVerticalCells = 0;
	int32 GuardRadiusCells = 0;
	int32 WorldMinZ = 0;
	int32 WorldMaxZ = 0;
	uint64 NextWindowSerial = 0;
	uint64 RuntimeDemandRevision = 0;
	uint64 ResidencyRevision = 0;
	uint64 PriorityRevision = 0;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
};

// 多源只合并需求租约，源移除不会撤销其他玩家仍使用的 Shard。
class WHFRAMEWORK_API FVoxelFineResidencyLeases
{
public:
	TSharedPtr<FVoxelFineResidencyLeases, ESPMode::ThreadSafe> Capture(
		TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer, FString& OutError) const;
	bool TrySetSource(const FGuid& InSource, const TSet<FVoxelFineShardKey>& InShards,
		TVoxelFineSetDelta<FVoxelFineShardKey>& OutDelta, FString& OutError);
	bool TryRemoveSource(const FGuid& InSource, TVoxelFineSetDelta<FVoxelFineShardKey>& OutDelta, FString& OutError);
	uint64 GetAllocatedBytes() const;
	TVoxelFineSetDelta<FVoxelFineShardKey> SetSource(const FGuid& InSource, const TSet<FVoxelFineShardKey>& InShards);
	TVoxelFineSetDelta<FVoxelFineShardKey> RemoveSource(const FGuid& InSource);
	bool IsRetained(const FVoxelFineShardKey& InKey) const;
	int32 ReferenceCount(const FVoxelFineShardKey& InKey) const;
	void Reset();

private:
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> StorageLease;
	TMap<FGuid, TSet<FVoxelFineShardKey>> Sources;
	TMap<FVoxelFineShardKey, int32> Counts;
};
