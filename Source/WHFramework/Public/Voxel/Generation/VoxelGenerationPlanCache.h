#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"

#include "Voxel/Generation/Caves/VoxelCaveGenerator.h"
#include "Voxel/Generation/Ecology/VoxelEcology.h"
#include "Voxel/Generation/Hydrology/VoxelHydrology.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyRegionGraph.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyNetworkSearch.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyChannelPlan.h"
#include "Voxel/Generation/VoxelFeaturePlan.h"
#include "Voxel/Generation/VoxelNaturalGenerationCache.h"
#include "Voxel/Generation/VoxelStructurePlan.h"
#include "Voxel/Generation/VoxelPlanDiagnostics.h"
#include "Voxel/Generation/VoxelGenerationConsumer.h"

struct FVoxelGenerationRuntimeConfig;
struct FVoxelHydrologyRiverComponent;
class FVoxelClimateGenerator;
class FVoxelTerrainGenerator;

using FVoxelHydrologyPlanPtr =
	TSharedPtr<
		const FVoxelHydrologyPlan,
		ESPMode::ThreadSafe>;

using FVoxelHydrologyRegionGraphPtr = TSharedPtr<const FVoxelHydrologyRegionGraph, ESPMode::ThreadSafe>;

using FVoxelCavePlanPtr =
	TSharedPtr<
		const FVoxelCavePlan,
		ESPMode::ThreadSafe>;

using FVoxelFeaturePlanPtr =
	TSharedPtr<
		const FVoxelFeaturePlan,
		ESPMode::ThreadSafe>;

using FVoxelEcologyPlanPtr =
	TSharedPtr<
		const FVoxelEcologyPlan,
		ESPMode::ThreadSafe>;

using FVoxelStructurePlanPtr =
	TSharedPtr<
		const FVoxelStructurePlan,
		ESPMode::ThreadSafe>;

using FVoxelBaseColumnEntryPtr =
	TSharedPtr<
		const FVoxelBaseColumnEntry,
		ESPMode::ThreadSafe>;

using FVoxelNaturalColumnEntryPtr =
	TSharedPtr<
		const FVoxelNaturalColumnEntry,
		ESPMode::ThreadSafe>;

struct WHFRAMEWORK_API FVoxelGenerationCacheRetentionPoint
{
	FIntPoint Center = FIntPoint::ZeroValue;
	int32 NaturalRadiusCells = 0;
	int32 PlanRadiusCells = 0;
	int32 HydrologyRadiusCells = 0;
};

struct WHFRAMEWORK_API FVoxelGenerationCacheRetention
{
	TArray<FVoxelGenerationCacheRetentionPoint> Points;
	int32 HydrologyRegionSide = 256;
	int32 HydrologyCellSize = 1;

	uint64 Revision = 0;
};

class FVoxelGenerationPlanCache;
class FVoxelTerrainResources;
class FVoxelTerrainResourceLease;
struct FVoxelGenerationConsumerKey;

/** 最后一个任务引用释放后解链；原资源授予覆盖到对象及控制器实际退出。 */
class WHFRAMEWORK_API FVoxelGenerationCacheRetentionLease
{
private:
	friend class FVoxelGenerationPlanCache;
	FVoxelGenerationCacheRetentionLease(
		TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> InCache,
		TSharedRef<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InResourceLease,
		const FVoxelGenerationCacheRetentionPoint& InPoint,
		uint64 InId);

	struct FDeleter
	{
		TSharedRef<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
		void operator()(FVoxelGenerationCacheRetentionLease* InLease) const;
	};

public:
	~FVoxelGenerationCacheRetentionLease();
	FVoxelGenerationCacheRetentionLease(const FVoxelGenerationCacheRetentionLease&) = delete;
	FVoxelGenerationCacheRetentionLease& operator=(const FVoxelGenerationCacheRetentionLease&) = delete;

	static uint64 GetRequiredAllocationBytes();
	const TSharedRef<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& GetResourceLease() const
	{
		return ResourceLease;
	}

private:
	TSharedRef<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
	FVoxelGenerationCacheRetentionPoint Point;
	FVoxelGenerationCacheRetentionLease* Previous = nullptr;
	FVoxelGenerationCacheRetentionLease* Next = nullptr;
	uint64 Id = 0;
};

struct WHFRAMEWORK_API FVoxelGenerationCacheStats
{
	int32 BaseColumns = 0;
	int32 NaturalColumns = 0;
	int32 Hydrology = 0;
	int32 HydrologyGraphs = 0;
	int32 HydrologyControls = 0;
	int32 Caves = 0;
	int32 Ecology = 0;
	int32 Features = 0;
	int32 Structures = 0;

	uint64 AllocatedBytes = 0;

	uint64 GateWaitCount = 0;
	uint64 GateWaitMicroseconds = 0;
};

class WHFRAMEWORK_API FVoxelGenerationPlanCache :
	public TSharedFromThis<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>
{
public:
	static constexpr int32 ShardCount = 32;

public:
	/** 原 World 账本在分配前授予根对象、控制器与 shard；拒收保留 OutCache，绑定终生不替换。 */
	static bool Create(
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InControlConsumer,
		TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>& OutCache,
		FString& OutError,
		bool bInAllowGameThreadBuilds = false,
		bool bInAllowWorkerWait = false);

#if WITH_DEV_AUTOMATION_TESTS
	/** 仅保留无账本的旧夹具；生产入口必须使用 Create。 */
	explicit FVoxelGenerationPlanCache(
		bool bInAllowGameThreadBuilds = true,
		bool bInAllowWorkerWait = true);
#endif

	~FVoxelGenerationPlanCache();

	const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& GetResources() const;
	const FVoxelGenerationConsumerKey& GetControlConsumer() const;
	uint64 GetRootInputBytes() const { return RootInputBytes; }
	/** 根和当前控制 backing 的同锁快照；不含可淘汰的已发布正文。 */
	void GetRequiredInputCapacity(uint64& OutBytes, int32& OutLeaseCount) const;
	void MarkPublished() const;
	/** 解析原 TLS 完整身份；无 TLS 使用冻结 Control，拒收不修改 OutConsumer。 */
	bool ResolveConsumer(FVoxelGenerationConsumerKey& OutConsumer, FString& OutError) const;

	bool GetOrBuildBaseColumn(
		const FIntPoint& InPosition,
		TFunctionRef<bool(
			FVoxelBaseColumnEntry&,
			FString&)> InBuild,
		FVoxelBaseColumnEntryPtr& OutEntry,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);

	bool GetOrBuildNaturalColumn(
		const FIntPoint& InPosition,
		TFunctionRef<bool(
			FVoxelNaturalColumnEntry&,
			FString&)> InBuild,
		FVoxelNaturalColumnEntryPtr& OutEntry,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);

	bool FindNaturalColumn(const FIntPoint& InPosition, FVoxelNaturalColumnEntryPtr& OutEntry) const;

	bool FindHydrology(
		const FVoxelHydrologyRegionKey& InKey,
		FVoxelHydrologyPlanPtr& OutPlan) const;

	/** 各 Store* 同 key 首个 immutable 胜出；bound cache 拒收无法证明原租约的外来新正文，旧夹具先授予控制存储。 */
	bool StoreHydrology(
		const FVoxelHydrologyRegionKey& InKey,
		FVoxelHydrologyPlanPtr InPlan,
		FString* OutError = nullptr);

	bool GetOrBuildHydrology(
		const FVoxelHydrologyRegionKey& InKey,
		TFunctionRef<bool(
			FVoxelHydrologyPlan&,
			FString&)> InBuild,
		FVoxelHydrologyPlanPtr& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);

	bool BindHydrologyRecipe(uint64 InRecipeHash, uint32 InAlgorithmVersion, FString& OutError, uint64 InExpectedGeneration = 0);
	uint64 GetHydrologyGeneration() const { return HydrologyGeneration.Load(); }

	// 单次只推进一个有界批次；未 Ready 保持原 owner 和容量，不发布 Region。
	bool AdvanceHydrologyPlan(const FVoxelHydrologyRegionKey& InKey,
		TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		TSharedRef<const FVoxelClimateGenerator, ESPMode::ThreadSafe> InClimate,
		TSharedRef<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> InTerrain,
		FVoxelHydrologyPlanPtr& OutPlan, FString& OutError, const TAtomic<bool>* InCancel = nullptr);

	void CancelHydrologyBuild(const FVoxelHydrologyRegionKey& InKey, uint64 InWorldEpoch,
		const FGuid& InOwnerId, uint64 InOwnerIncarnation, uint64 InAttemptId);

	// 宏观拓扑输入与正式覆盖计划分别存储，由原 owner 续跑并发布。
	bool FindHydrologyTopology(const FVoxelHydrologyRegionKey& InKey, FVoxelHydrologyPlanPtr& OutPlan) const;
	bool GetOrBuildHydrologyTopology(const FVoxelHydrologyRegionKey& InKey,
		TFunctionRef<bool(FVoxelHydrologyPlan&, FString&)> InBuild,
		FVoxelHydrologyPlanPtr& OutPlan, FString& OutError, const TAtomic<bool>* InCancel = nullptr);

	bool FindHydrologyGraph(const FVoxelHydrologyRegionKey& InKey, FVoxelHydrologyRegionGraphPtr& OutGraph) const;
	bool AdvanceHydrologyGraph(const FVoxelHydrologyRegionKey& InKey,
		TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		FVoxelHydrologyRegionGraphBuild::FReadCell InReadCell, FVoxelHydrologyRegionGraphPtr& OutGraph,
		FString& OutError, const TAtomic<bool>* InCancel = nullptr);
	bool AdvanceHydrologyGraph(const FVoxelHydrologyRegionKey& InKey,
		TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		FVoxelHydrologyMacroInput::FTileLookup InLookup, FVoxelHydrologyMacroInput::FRequestTile InRequestTile,
		FVoxelHydrologyRegionGraphPtr& OutGraph, FString& OutError, const TAtomic<bool>* InCancel = nullptr);
	void CancelHydrologyGraphBuild(const FVoxelHydrologyRegionKey& InKey, uint64 InWorldEpoch,
		const FGuid& InOwnerId, uint64 InOwnerIncarnation, uint64 InAttemptId);

	// 只借用已发布且实际覆盖请求节点的同代证明；不启动构建或建立第二份索引。
	bool FindHydrologyControl(const FIntPoint& InCell, const FVoxelGenerationRuntimeConfig& InConfig, uint64 InGeneration,
		TSharedPtr<const FVoxelHydrologyNetworkResult, ESPMode::ThreadSafe>& OutResult) const;
	bool AdvanceHydrologyControl(const FVoxelHydrologyNetworkKey& InKey,
		TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		FVoxelHydrologyMacroInput::FTileLookup InLookup, FVoxelHydrologyMacroInput::FRequestTile InRequestTile,
		TSharedPtr<const FVoxelHydrologyNetworkResult, ESPMode::ThreadSafe>& OutResult,
		FString& OutError, const TAtomic<bool>* InCancel = nullptr, double InDeadlineSeconds = 0.0);

	bool AdvanceHydrologyRunoff(const FVoxelHydrologyRegionKey& InKey,
		TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		FVoxelHydrologyMacroInput::FTileLookup InLookup, FVoxelHydrologyMacroInput::FRequestTile InRequestTile,
		TSharedPtr<const FVoxelHydrologyNetworkRunoff, ESPMode::ThreadSafe>& OutResult,
		FString& OutError, const TAtomic<bool>* InCancel = nullptr);
	bool AdvanceHydrologySourceBucket(const FVoxelHydrologyRegionKey& InKey,
		TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		FVoxelHydrologyMacroInput::FTileLookup InLookup, FVoxelHydrologyMacroInput::FRequestTile InRequestTile,
		TSharedPtr<const FVoxelHydrologySourceBucket, ESPMode::ThreadSafe>& OutResult,
		FString& OutError, const TAtomic<bool>* InCancel = nullptr);
	bool AdvanceHydrologySources(const FVoxelHydrologyRegionKey& InKey,
		TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		FVoxelHydrologyMacroInput::FTileLookup InLookup, FVoxelHydrologyMacroInput::FRequestTile InRequestTile,
		TSharedPtr<const FVoxelHydrologySourcePlan, ESPMode::ThreadSafe>& OutResult,
		FString& OutError, const TAtomic<bool>* InCancel = nullptr);
	bool AdvanceHydrologyChannels(const FVoxelHydrologyRegionKey& InKey,
		TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		FVoxelHydrologyMacroInput::FTileLookup InLookup, FVoxelHydrologyMacroInput::FRequestTile InRequestTile,
		TSharedPtr<const FVoxelHydrologyChannelPlan, ESPMode::ThreadSafe>& OutResult,
		FString& OutError, const TAtomic<bool>* InCancel = nullptr);
	bool AdvanceHydrologyComponent(const FVoxelHydrologyRegionKey& InKey,
		TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		FVoxelHydrologyMacroInput::FTileLookup InLookup, FVoxelHydrologyMacroInput::FRequestTile InRequestTile,
		TSharedPtr<const FVoxelHydrologyRiverComponent, ESPMode::ThreadSafe>& OutResult,
		FString& OutError, const TAtomic<bool>* InCancel = nullptr);

	bool FindCave(
		const FVoxelGenerationTileKey& InKey,
		FVoxelCavePlanPtr& OutPlan) const;

	bool StoreCave(
		const FVoxelGenerationTileKey& InKey,
		FVoxelCavePlanPtr InPlan,
		FString* OutError = nullptr);

	bool GetOrBuildCave(
		const FVoxelGenerationTileKey& InKey,
		TFunctionRef<bool(
			FVoxelCavePlan&,
			FString&)> InBuild,
		FVoxelCavePlanPtr& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);

	bool FindFeature(
		const FVoxelGenerationTileKey& InKey,
		FVoxelFeaturePlanPtr& OutPlan) const;

	bool FindEcology(
		const FVoxelEcologyTileKey& InKey,
		FVoxelEcologyPlanPtr& OutPlan) const;

	bool StoreFeature(
		const FVoxelGenerationTileKey& InKey,
		FVoxelFeaturePlanPtr InPlan,
		FString* OutError = nullptr);

	bool GetOrBuildFeature(
		const FVoxelGenerationTileKey& InKey,
		TFunctionRef<bool(
			FVoxelFeaturePlan&,
			FString&)> InBuild,
		FVoxelFeaturePlanPtr& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);

	bool GetOrBuildEcology(
		const FVoxelEcologyTileKey& InKey,
		TFunctionRef<bool(FVoxelEcologyPlan&, FString&)> InBuild,
		FVoxelEcologyPlanPtr& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);

	bool FindStructure(
		const FVoxelGenerationTileKey& InKey,
		FVoxelStructurePlanPtr& OutPlan) const;

	bool StoreStructure(
		const FVoxelGenerationTileKey& InKey,
		FVoxelStructurePlanPtr InPlan,
		FString* OutError = nullptr);

	bool GetOrBuildStructure(
		const FVoxelGenerationTileKey& InKey,
		TFunctionRef<bool(
			FVoxelStructurePlan&,
			FString&)> InBuild,
		FVoxelStructurePlanPtr& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);

	/** 冻结来源按 Count/Fill 写入已预授额的快照；拒收或取消保留旧正文与租约。 */
	bool UpdateRetention(
		const FVoxelGenerationCacheRetention& InRetention,
		int32 InPointCount,
		TFunctionRef<bool(TArrayView<FVoxelGenerationCacheRetentionPoint>, FString&)> InFill,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);

#if WITH_DEV_AUTOMATION_TESTS
	/** 旧夹具的数组入口；生产使用冻结来源的 Count/Fill 接口。 */
	void UpdateRetention(const FVoxelGenerationCacheRetention& InRetention);
#endif

	/** 从调用方原Input池授予对象及控制器容量；拒收保留OutLease，旧节点按实际生命期解链退租。 */
	bool RetainForTask(
		const FVoxelGenerationCacheRetentionPoint& InPoint,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe>& OutLease,
		FString& OutError);

	/**
	 * 必须是有预算的增量维护。
	 * 允许 GameThread 调用，但单帧最多检查 MaxEntries。
	 */
	void TickMaintenance(
		int32 InMaxEntries = 256);

	/** 终结当时在途的原 gate；外部 immutable 引用保持有效，不等待 worker。gate 诊断保留前1023 TCHAR。 */
	void Reset();

	FVoxelGenerationCacheStats GetStats() const;
	FVoxelPlanDiagnostics GetPlanDiagnostics(EVoxelGenerationPlanKind InKind) const;
	void RecordPlanDonation(EVoxelGenerationPlanKind InKind);
	uint64 GetAllocatedBytes() const;

private:
	FVoxelGenerationPlanCache(
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InControlConsumer,
		bool bInAllowGameThreadBuilds,
		bool bInAllowWorkerWait);
	void InitializeShards();

	friend class FVoxelGenerationCacheRetentionLease;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FVoxelCacheLifecycleTestAccess;
#endif
	void ReleaseTaskRetention(FVoxelGenerationCacheRetentionLease& InLease);

	struct FBuildGate;
	struct FHydrologyRegionOwner;
	struct FHydrologyGraphOwner;
	struct FHydrologyFactOwnerBase;
	template<typename BuildType, typename ResultType> struct THydrologyFactOwner;

	struct FBackingReservation
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ValuesLease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> BuildsLease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> KeysLease;
		int32 ValuesCapacity = 0;
		int32 BuildsCapacity = 0;
		int32 KeysCapacity = 0;
	};

	struct FNaturalShard
	{
		mutable FRWLock Lock;
		// 租约在容器前声明，原 native backing 先析构后退还容量。
		FBackingReservation BaseColumnBacking;
		FBackingReservation NaturalColumnBacking;

		TMap<FIntPoint, FVoxelBaseColumnEntryPtr> BaseColumns;
		TMap<FIntPoint, FVoxelNaturalColumnEntryPtr> NaturalColumns;

		TMap<FIntPoint, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>> BaseColumnBuilds;
		TMap<FIntPoint, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>> NaturalColumnBuilds;

		TArray<FIntPoint> BaseColumnKeys;
		TArray<FIntPoint> NaturalColumnKeys;

		int32 BaseColumnCursor = 0;
		int32 NaturalColumnCursor = 0;
	};

	template<typename ResultType, typename KeyType = FVoxelHydrologyRegionKey>
	struct THydrologyFactStorage
	{
		FBackingReservation Backing;
		TMap<KeyType, TSharedPtr<const ResultType, ESPMode::ThreadSafe>> Values;
		TMap<KeyType, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>> Builds;
		TArray<KeyType> Keys;
		int32 Cursor = 0;
	};

	struct FPlanShard
	{
		mutable FRWLock Lock;
		THydrologyFactStorage<FVoxelHydrologyNetworkResult, FVoxelHydrologyNetworkKey> HydrologyControls;
		THydrologyFactStorage<FVoxelHydrologyNetworkRunoff> HydrologyRunoff;
		THydrologyFactStorage<FVoxelHydrologySourceBucket> HydrologySourceBucket;
		THydrologyFactStorage<FVoxelHydrologySourcePlan> HydrologySources;
		THydrologyFactStorage<FVoxelHydrologyChannelPlan> HydrologyChannels;
		THydrologyFactStorage<FVoxelHydrologyRiverComponent> RiverComponents;
		FBackingReservation HydrologyBacking;
		FBackingReservation HydrologyTopologyBacking;
		FBackingReservation HydrologyGraphBacking;
		FBackingReservation CaveBacking;
		FBackingReservation EcologyBacking;
		FBackingReservation FeatureBacking;
		FBackingReservation StructureBacking;

		TMap<FVoxelHydrologyRegionKey, FVoxelHydrologyPlanPtr> Hydrology;
		TMap<FVoxelHydrologyRegionKey, FVoxelHydrologyPlanPtr> HydrologyTopology;
		TMap<FVoxelHydrologyRegionKey, FVoxelHydrologyRegionGraphPtr> HydrologyGraphs;
		TMap<FVoxelGenerationTileKey, FVoxelCavePlanPtr> Caves;
		TMap<FVoxelEcologyTileKey, FVoxelEcologyPlanPtr> Ecology;
		TMap<FVoxelGenerationTileKey, FVoxelFeaturePlanPtr> Features;
		TMap<FVoxelGenerationTileKey, FVoxelStructurePlanPtr> Structures;

		TMap<FVoxelHydrologyRegionKey, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>> HydrologyBuilds;
		TMap<FVoxelHydrologyRegionKey, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>> HydrologyTopologyBuilds;
		TMap<FVoxelHydrologyRegionKey, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>> HydrologyGraphBuilds;
		TMap<FVoxelGenerationTileKey, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>> CaveBuilds;
		TMap<FVoxelEcologyTileKey, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>> EcologyBuilds;
		TMap<FVoxelGenerationTileKey, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>> FeatureBuilds;
		TMap<FVoxelGenerationTileKey, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>> StructureBuilds;

		TArray<FVoxelHydrologyRegionKey> HydrologyKeys;
		TArray<FVoxelHydrologyRegionKey> HydrologyTopologyKeys;
		TArray<FVoxelHydrologyRegionKey> HydrologyGraphKeys;
		TArray<FVoxelGenerationTileKey> CaveKeys;
		TArray<FVoxelEcologyTileKey> EcologyKeys;
		TArray<FVoxelGenerationTileKey> FeatureKeys;
		TArray<FVoxelGenerationTileKey> StructureKeys;

		int32 HydrologyCursor = 0;
		int32 HydrologyTopologyCursor = 0;
		int32 HydrologyGraphCursor = 0;
		int32 CaveCursor = 0;
		int32 EcologyCursor = 0;
		int32 FeatureCursor = 0;
		int32 StructureCursor = 0;
	};

	template<typename BuildType, typename ResultType, typename KeyType>
	bool AdvanceHydrologyFact(const KeyType& InKey,
		TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		THydrologyFactStorage<ResultType, KeyType> FPlanShard::* InStorage,
		TFunctionRef<bool(THydrologyFactOwner<BuildType, ResultType>&, FString&)> InInitialize,
		TFunctionRef<EVoxelHydrologyRegionStep(THydrologyFactOwner<BuildType, ResultType>&, FString&)> InAdvance,
		TSharedPtr<const ResultType, ESPMode::ThreadSafe>& OutResult, FString& OutError, const TAtomic<bool>* InCancel);

    void CancelHydrologyDependencies(uint64 InWorldEpoch, const FGuid& InOwnerId,
        uint64 InOwnerIncarnation, uint64 InAttemptId);

	int32 NaturalShardIndex(const FIntPoint& InKey) const;

	int32 PlanShardIndex(const FVoxelHydrologyRegionKey& InKey) const;
	int32 PlanShardIndex(const FVoxelHydrologyNetworkKey& InKey) const;
	int32 PlanShardIndex(const FVoxelGenerationTileKey& InKey) const;
	int32 PlanShardIndex(const FVoxelEcologyTileKey& InKey) const;

	bool IsRetained(
		const FInt64Vector2& InPosition,
		int32 FVoxelGenerationCacheRetentionPoint::* InRadius) const;

	void RecordGateWait(uint64 InMicroseconds);
	void RecordPlanLookup(EVoxelGenerationPlanKind InKind, bool bInHit) const;
	void RecordPlanBuild(EVoxelGenerationPlanKind InKind, double InMilliseconds, bool bInFailed);

private:
	struct FControlBackingCapacity
	{
		mutable FRWLock Lock;
		uint64 Bytes = 0;
		int32 LeaseCount = 0;
		void Replace(const FVoxelTerrainResourceLease* InOldLease, const FVoxelTerrainResourceLease* InNewLease);
	};
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RootLease;
	FControlBackingCapacity ControlBackingCapacity;
	FVoxelGenerationConsumerKey ControlConsumer;
	uint64 RootInputBytes = 0;
	TArray<TUniquePtr<FNaturalShard>> NaturalShards;
	TArray<TUniquePtr<FPlanShard>> PlanShards;

	mutable FRWLock RetentionLock;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RetentionLease;
	FVoxelGenerationCacheRetention Retention;
	FVoxelGenerationCacheRetentionLease* TaskRetentionHead = nullptr;
	FVoxelGenerationCacheRetentionLease* TaskRetentionTail = nullptr;
	uint64 NextTaskRetentionId = 1;

	int32 MaintenanceShardCursor = 0;

	TAtomic<uint64> GateWaitCount { 0 };
	TAtomic<uint64> GateWaitMicroseconds { 0 };
	mutable TAtomic<uint64> PlanHits[5];
	mutable TAtomic<uint64> PlanMisses[5];
	TAtomic<uint64> PlanDonations[5];
	mutable FRWLock PlanDiagnosticsLock;
	FVoxelPlanDiagnostics PlanBuildDiagnostics[5];

	TAtomic<uint64> HydrologyGeneration{1};
	TAtomic<bool> bHydrologyResetting{false};
	uint64 HydrologyRecipeHash = 0;
	uint32 HydrologyAlgorithmVersion = 0;

	bool bAllowGameThreadBuilds = true;
	bool bAllowWorkerWait = true;
};
