#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"
#include "Voxel/Generation/VoxelGenerationOverlay.h"

class FVoxelGenerationWorkspace;

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

	// 调用者先授予并构造输出 span；原事实 Working 候选完成后才复制，失败保持旧输出。
	// heap span 的原租约须由调用者强持到返回；同域租约计入必需峰值，nullptr 仅表示栈或另资源域输出。
	bool GenerateSection(
		const FIntVector& InSectionCoordinate,
		TArrayView<FVoxelBlockState> OutBaseBlocks,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr,
		const FVoxelTerrainResourceLease* InOutputLease = nullptr) const;

#if WITH_DEV_AUTOMATION_TESTS
	// 只兼容无账本夹具；绑定原账本时拒绝隐式分配 caller 数组。
	bool GenerateSection(
		const FIntVector& InSectionCoordinate,
		TArray<FVoxelBlockState>& OutBaseBlocks,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;
#endif

	bool SampleColumn(int32 InX, int32 InY, FVoxelColumnSample& OutColumn, FString& OutError, const TAtomic<bool>* InCancel = nullptr) const;

	bool EnumerateTrees(const FVoxelGenerationBounds& InBounds, TFunctionRef<void(const FIntVector&, int32, FVoxelStableId)> InVisit, FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	bool SampleEnvironment(int32 InX, int32 InY, FVoxelEnvironmentSample& OutSample, FString& OutError, const TAtomic<bool>* InCancel = nullptr) const;

	bool TrySampleEnvironment(const FIntPoint& InCell, FVoxelEnvironmentSample& OutSample) const;
	// 所有原生读取和 scratch 接纳先完成；visitor 失败可能接收前缀，调用者应在整体成功后发布候选。
	// Source 仅在同步 callback 内有效；调用者扩容前继承其必需输入、租约数量及 Working 下限。
	bool QueryReadyWaterfalls(
		const FIntPoint& InMin,
		const FIntPoint& InMax,
		TFunctionRef<bool(const FVoxelWaterfallSemantic&, const FVoxelGenerationWorkspace&, FString&)> InVisit,
		FString& OutError,
		const FVoxelTerrainResourceLease* InOutputLease = nullptr) const;

	bool EnumerateCaveEntrances(
		const FIntPoint& InMin,
		const FIntPoint& InMax,
		TFunctionRef<bool(const FVoxelCaveEntrance&, const FVoxelGenerationWorkspace&, FString&)> InVisit,
		FString& OutError,
		const FVoxelTerrainResourceLease* InOutputLease = nullptr) const;

#if WITH_DEV_AUTOMATION_TESTS
	bool QueryReadyWaterfalls(
		const FIntPoint& InMin,
		const FIntPoint& InMax,
		TArray<FVoxelWaterfallSemantic>& OutWaterfalls,
		FString& OutError) const;

	bool EnumerateCaveEntrances(
		const FIntPoint& InMin,
		const FIntPoint& InMax,
		TArray<FVoxelCaveEntrance>& OutEntrances,
		FString& OutError) const;
#endif
	bool RequiresPlanPreflight() const
	{
		return bRequirePlanPreflight;
	}

	const TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>& GetPlanCache() const
	{
		return Cache;
	}

	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> RetainHydrologyForPlanning(
		const FIntPoint& InCenter,
		int32 InSampleRadiusCells) const;

	bool SampleEnvironments(
		const FIntPoint& InOrigin,
		int32 InWidth,
		int32 InHeight,
		int32 InStep,
		TArrayView<FVoxelEnvironmentSample> OutSamples,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr,
		bool bInUseColumnCache = true,
		const FVoxelTerrainResourceLease* InOutputLease = nullptr) const;

	bool SampleColumns(
		const FIntPoint& InOrigin,
		int32 InWidth,
		int32 InHeight,
		int32 InStep,
		TArrayView<FVoxelColumnSample> OutColumns,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr,
		bool bInUseColumnCache = true,
		const FVoxelTerrainResourceLease* InOutputLease = nullptr) const;

#if WITH_DEV_AUTOMATION_TESTS
	// 输出 span 长度为 Width * Height；与 GenerateSection 使用相同的原租约和事务输出契约。
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
#endif

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
	bool ResolveAllocation(
		TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& OutResources,
		FVoxelGenerationConsumerKey& OutConsumer,
		FString& OutError) const;

	bool AddOutputRequirements(
		const FVoxelTerrainResourceLease* InOutputLease,
		uint64& InOutRequiredInputBytes,
		int32& InOutRequiredLeaseCount,
		uint64& OutRequiredWorkingBytes,
		FString& OutError) const;

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
