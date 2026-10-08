#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"
#include "Voxel/Generation/VoxelGenerationOverlay.h"

class WHFRAMEWORK_API FVoxelGenerationPipeline
{
public:
	void SetOverlayPlanningServices(FVoxelGenerationPlanningServices InServices) const;
	void BindRetentionResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer) const;
	void NotifyOverlayPlanningDependencyReady(const FVoxelGenerationConsumerKey& InConsumer) const;
	FVoxelOverlayPreflightStatus EnsureOverlayConsumerReady(
		const FVoxelGenerationConsumerKey& InConsumer,
		const FVoxelGenerationBounds& InBounds,
		EVoxelWorkClass InWorkClass,
		const FVoxelTerrainServicePriority& InPriority) const;
	void ReleaseOverlayConsumer(const FVoxelGenerationConsumerKey& InConsumer) const;
	void SetTypedOverlayConsumerWakeCallback(
		TFunction<void(const FVoxelGenerationConsumerKey&)> InCallback) const;
	void SetOverlayServicePriorityDonation(
		TFunction<void(const FVoxelTaskStamp&, EVoxelWorkClass, const FVoxelTerrainServicePriority&)> InCallback) const;
	void AdvanceOverlayConsumerWakes(double InDeadlineSeconds, int32 InLimit) const;
	void SetOverlayConsumerWakeCallback(TFunction<void(const FIntVector&)> InCallback) const;
	void ReleaseOverlaySectionConsumer(const FIntVector& InSection) const;
	FVoxelOverlayPreflightStatus EnsureOverlayBoundsReady(const FVoxelGenerationBounds& InBounds, EVoxelWorkClass InWorkClass, int32 InSourcePriority) const;
	FVoxelOverlayPreflightStatus EnsureCoarseOverlayBoundsReady(
		const FVoxelGenerationBounds& InBounds, EVoxelWorkClass InWorkClass, int32 InSourcePriority) const;
	FVoxelOverlayPreflightStatus EnsureOverlaySectionReady(
		const FIntVector& InSectionCoordinate, EVoxelWorkClass InWorkClass, int32 InSourcePriority, double InDistanceScore, double InForwardScore) const;

	FVoxelGenerationPipeline(TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> InCache, TSharedPtr<const IVoxelGenerationOverlay, ESPMode::ThreadSafe> InOverlay = nullptr,
		bool bInRequirePlanPreflight = false);

	bool GenerateSection(
		const FIntVector& InSectionCoordinate, TArray<FVoxelBlockState>& OutBaseBlocks, FString& OutError, const TAtomic<bool>* InCancel = nullptr) const;

	bool SampleColumn(int32 InX, int32 InY, FVoxelColumnSample& OutColumn, FString& OutError, const TAtomic<bool>* InCancel = nullptr) const;

	bool EnumerateTrees(const FVoxelGenerationBounds& InBounds, TFunctionRef<void(const FIntVector&, int32, FVoxelStableId)> InVisit, FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	bool SampleEnvironment(int32 InX, int32 InY, FVoxelEnvironmentSample& OutSample, FString& OutError, const TAtomic<bool>* InCancel = nullptr) const;

	bool TrySampleEnvironment(const FIntPoint& InCell, FVoxelEnvironmentSample& OutSample) const;
	void QueryReadyWaterfalls(const FIntPoint& InMin, const FIntPoint& InMax, TArray<FVoxelWaterfallSemantic>& OutWaterfalls) const;
	bool EnumerateCaveEntrances(const FIntPoint& InMin, const FIntPoint& InMax, TArray<FVoxelCaveEntrance>& OutEntrances, FString& OutError) const;
	bool RequiresPlanPreflight() const
	{
		return bRequirePlanPreflight;
	}

	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> RetainHydrologyForPlanning(
		const FIntPoint& InCenter,
		int32 InSampleRadiusCells) const;

	bool SampleEnvironments(
		const FIntPoint& InOrigin,
		int32 InWidth,
		int32 InHeight,
		int32 InStep,
		TArray<FVoxelEnvironmentSample>& OutSamples,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr,
		bool bInUseColumnCache = true) const;

	bool SampleColumns(
		const FIntPoint& InOrigin,
		int32 InWidth,
		int32 InHeight,
		int32 InStep,
		TArray<FVoxelColumnSample>& OutColumns,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr,
		bool bInUseColumnCache = true) const;

	bool SampleBlock(
		const FIntVector& InPosition,
		FVoxelBlockState& OutState,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	bool BuildCoarseOverlay(
		const FIntVector& InOrigin,
		int32 InStep,
		int32 InGridSide,
		TMap<FIntVector, FVoxelBlockState>& OutCells,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

private:
	TSharedRef<
		const FVoxelGenerationRuntimeConfig,
		ESPMode::ThreadSafe> Config;

	TSharedRef<
		FVoxelGenerationPlanCache,
		ESPMode::ThreadSafe> Cache;

	TSharedPtr<const IVoxelGenerationOverlay, ESPMode::ThreadSafe> Overlay;
	bool bRequirePlanPreflight = false;
	// 绑定一次原世界账本；Stop 清空 Overlay 服务时不替换在途 Worker 的原所有者。
	mutable TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> RetentionResources;
	mutable FVoxelGenerationConsumerKey RetentionConsumer;
};
