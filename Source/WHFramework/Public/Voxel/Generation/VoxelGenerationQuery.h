#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"

class FVoxelAquiferGenerator;
class FVoxelBiomeGenerator;
class FVoxelCaveGenerator;
class FVoxelClimateGenerator;
class FVoxelEcologyGenerator;
class FVoxelHydrologyGenerator;
using FVoxelHydrologyPlanner = FVoxelHydrologyGenerator;
class FVoxelSurfaceGenerator;
class FVoxelTerrainGenerator;

#if WITH_DEV_AUTOMATION_TESTS
struct WHFRAMEWORK_API FVoxelGenerationPlanKeys
{
	TArray<FVoxelGenerationTileKey> Tiles;
	TArray<FVoxelEcologyTileKey> EcologyTiles;
};
#endif

struct WHFRAMEWORK_API FVoxelGenerationPlanKeyRanges
{
	FIntPoint TileMin;
	FIntPoint TileMax;
	FIntPoint EcologyMin;
	FIntPoint EcologyMax;
	int32 TileCount = 0;
	int32 EcologyCount = 0;
};

struct WHFRAMEWORK_API FVoxelGenerationStructureKeyRange
{
	FIntPoint Min;
	FIntPoint Max;
	int32 Count = 0;
};

class WHFRAMEWORK_API FVoxelGenerationQuery
{
public:
	FVoxelGenerationQuery();
	~FVoxelGenerationQuery();
	FVoxelGenerationQuery(const FVoxelGenerationQuery&) = delete;
	FVoxelGenerationQuery& operator=(const FVoxelGenerationQuery&) = delete;
	FVoxelGenerationQuery(FVoxelGenerationQuery&& InOther);
	FVoxelGenerationQuery& operator=(FVoxelGenerationQuery&& InOther);

	static constexpr int32 GenerationPlanTileSide = 256;
	static constexpr int32 EcologyTileSide = 64;

	// 只计算空间与数量，供原生容量接纳使用，不创建 Keys 正文。
	static bool GetPlanKeyRanges(const FVoxelGenerationBounds& InBounds,
		const FVoxelGenerationSettings& InSettings, FVoxelGenerationPlanKeyRanges& OutRanges, FString& OutError);
	static bool GetStructureKeyRange(
		const FVoxelGenerationBounds& InBounds,
		FVoxelGenerationStructureKeyRange& OutRange,
		FString& OutError);

#if WITH_DEV_AUTOMATION_TESTS
	// 兼容无账本夹具；生产按范围迭代，不返回无租约数组。
	static FVoxelGenerationPlanKeys GatherPlanKeys(
		const FVoxelGenerationBounds& InBounds,
		const FVoxelGenerationSettings& InSettings);

	static void GatherStructureTiles(
		const FVoxelGenerationBounds& InBounds,
		TArray<FVoxelGenerationTileKey>& OutTiles);
#endif

	static FVoxelHydrologyRegionKey HydrologyKeyForVoxel(
		int32 InX,
		int32 InY,
		const FVoxelGenerationSettings& InSettings, uint32 InAlgorithmVersion);

	static int64 HydrologyKeySide(const FVoxelGenerationSettings& InSettings, uint32 InAlgorithmVersion);
	static FVoxelHydrologyRegionKey HydrologyTopologyKeyForVoxel(
		int32 InX, int32 InY, const FVoxelGenerationSettings& InSettings);
	// 上下文由调用者事先授予，Query 只延长原强引用，不创建回调 heap 或控制器。
	void SetMissingHydrologyObserver(
		TSharedPtr<void, ESPMode::ThreadSafe> InContext,
		void (*InObserver)(void*, const FVoxelHydrologyRegionKey&))
	{
		MissingHydrologyContext = MoveTemp(InContext);
		MissingHydrologyObserver = InObserver;
	}

	bool EnsureFineWater(const FIntPoint& InCell, FVoxelHydrologyFinePlanPtr& OutPlan,
		FString& OutError, const TAtomic<bool>* InCancel = nullptr) const;

	bool EnsureHydrologyPlan(
		const FVoxelHydrologyRegionKey& InKey,
		FVoxelHydrologyPlanPtr& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	bool EnsureCavePlan(
		const FVoxelGenerationTileKey& InKey,
		FVoxelCavePlanPtr& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	bool EnsureStructurePlan(
		const FVoxelGenerationTileKey& InKey,
		FVoxelStructurePlanPtr& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	bool EnsureFeaturePlan(
		const FVoxelGenerationTileKey& InKey,
		FVoxelFeaturePlanPtr& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	bool EnsureEcologyPlan(
		const FVoxelEcologyTileKey& InKey,
		FVoxelEcologyPlanPtr& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	// 生产绑定原事实账本与显式或原 TLS 消费者；失败不替换 OutQuery。
	static bool Create(
		TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> InCache,
		FVoxelGenerationQuery& OutQuery,
		FString& OutError,
		bool bInUseColumnCache = true,
		bool bInRequireReadyHydrology = false,
		bool bInRequireReadyPlans = false,
		const FVoxelGenerationConsumerKey* InConsumer = nullptr,
		const FVoxelTerrainResourceLease* InRequiredOutputLease = nullptr);

	bool PrepareColumns(
		const FVoxelGenerationBounds& InBounds,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);
	bool PrepareEnvironmentColumnRead(
		int32 InX,
		int32 InY,
		FString& OutError);

	bool Prepare(
		const FVoxelGenerationBounds& InBounds,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);

	bool PrepareStructuresOnly(
		const FVoxelGenerationBounds& InBounds,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);

	bool SampleEnvironmentColumn(
		int32 InX,
		int32 InY,
		FVoxelColumnSample& OutColumn,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	bool ResolveSurfaceCandidate(
		int32 InX,
		int32 InY,
		FVoxelSurfaceCandidate& OutCandidate,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	bool SampleColumn(
		int32 InX,
		int32 InY,
		FVoxelColumnSample& OutColumn,
		FString& OutError) const;

	bool SampleSymbol(
		const FIntVector& InPosition,
		uint32& OutValue,
		FString& OutError) const;

	// 输出由调用者持有；失败可能已写入前缀，调用者应在整体成功后发布。
	bool SampleColumnSymbols(
		const FIntPoint& InColumn,
		int32 InMinZ,
		TArrayView<uint32> OutValues,
		FString& OutError) const;

#if WITH_DEV_AUTOMATION_TESTS
	// 仅兼容无账本夹具；绑定账本的 Query 必须使用调用者 span。
	bool SampleColumnSymbols(const FIntPoint& InColumn, int32 InMinZ, int32 InCount,
		TArray<uint32>& OutValues, FString& OutError) const;
#endif

	TConstArrayView<FVoxelStructurePlanPtr>
		GetPreparedStructurePlans() const;
	uint64 GetRequiredInputBytes() const;
	int32 GetRequiredInputLeaseCount() const;
	// 原读者强持的 lease/native immutable 来源，不转移所有权、不分配枚举 backing。
	void VisitRequiredSources(FVoxelGenerationRequiredSourceScope::FVisit InVisit, bool bInIncludeCallerOutput = true) const;

private:
	struct FPreparedState
	{
		FPreparedState();
		~FPreparedState();
		FPreparedState(const FPreparedState&) = delete;
		FPreparedState& operator=(const FPreparedState&) = delete;
		FPreparedState(FPreparedState&& InOther);
		FPreparedState& operator=(FPreparedState&& InOther);

		void Reset();
		bool Reserve(
			const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
			const FVoxelGenerationConsumerKey& InConsumer,
			int32 InTileCount,
			int32 InEcologyCount,
			bool bInStructuresOnly,
		uint64 InExistingInputBytes,
		int32 InRequiredLeaseCount,
			FString& OutError);

		// 数组先释放真实 backing，再销毁覆盖它们的原 Input 租约。
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> HydrologyRetention;
		TArray<FVoxelCavePlanPtr> Caves;
		TArray<FVoxelEcologyPlanPtr> Ecology;
		TArray<FVoxelFeaturePlanPtr> Features;
		TArray<FVoxelStructurePlanPtr> Structures;
		FVoxelGenerationBounds Bounds;
		const TAtomic<bool>* Cancel = nullptr;
		bool bColumnsPrepared = false;
		bool bSymbolsPrepared = false;
	};

	bool ValidateConsumer(FString& OutError) const;
	bool RetainHydrologyReads(
		const FVoxelGenerationBounds& InBounds,
		int64 InMargin,
		TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe>& OutRetention,
		FString& OutError) const;

	bool SamplePlanColumn(
		const FIntVector& InPosition,
		FVoxelColumnSample& OutColumn,
		const TAtomic<bool>* InCancel) const;

	bool SamplePlanSymbol(
		const FIntVector& InPosition,
		uint32& OutSymbol,
		const TAtomic<bool>* InCancel) const;

	bool ResolveSymbol(const FIntVector& InPosition, FVoxelColumnSample InColumn,
		uint32& OutValue, FString& OutError) const;

	bool GetBaseColumn(
		int32 InX,
		int32 InY,
		FVoxelColumnSample& OutColumn,
		FString& OutError,
		const TAtomic<bool>* InCancel) const;

	bool ComputeBaseColumn(
		int32 InX,
		int32 InY,
		FVoxelColumnSample& OutColumn) const;

	bool ComputeNaturalColumn(
		int32 InX,
		int32 InY,
		FVoxelColumnSample& OutColumn,
		FString& OutError,
		const TAtomic<bool>* InCancel) const;

	bool ApplyHydrology(
		int32 InX,
		int32 InY,
		FVoxelColumnSample& InOutColumn,
		FString& OutError,
		const TAtomic<bool>* InCancel) const;


	bool IsInsidePreparedXY(
		int32 InX,
		int32 InY) const;

	bool ApplyStage(
		EVoxelGenerationStage InStage,
		const FIntVector& InPosition,
		FVoxelColumnSample& InOutColumn,
		uint32& InOutValue,
		FString& OutError) const;

private:
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
	uint64 CacheGeneration = 0;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	uint64 GeneratorInputBytes = 0;
	// 同步调用者强持输出，Query 只冻结已授额下限，不接管其寿命。
	uint64 RequiredOutputInputBytes = 0;
	int32 RequiredOutputLeaseCount = 0;
	FVoxelGenerationRequiredSource RequiredOutputSource;
	TSharedPtr<const FVoxelClimateGenerator, ESPMode::ThreadSafe> Climate;
	TSharedPtr<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> Terrain;
	TSharedPtr<const FVoxelBiomeGenerator, ESPMode::ThreadSafe> Biome;
	TSharedPtr<const FVoxelHydrologyPlanner, ESPMode::ThreadSafe> Hydrology;
	TSharedPtr<const FVoxelCaveGenerator, ESPMode::ThreadSafe> Cave;
	TSharedPtr<const FVoxelAquiferGenerator, ESPMode::ThreadSafe> Aquifer;
	TSharedPtr<const FVoxelSurfaceGenerator, ESPMode::ThreadSafe> Surface;
	TSharedPtr<const FVoxelEcologyGenerator, ESPMode::ThreadSafe> Ecology;
	FPreparedState Prepared;
	const FPreparedState* Preparing = nullptr;
	bool bUseColumnCache = true;
	bool bRequireReadyHydrology = false;
	bool bRequireReadyPlans = false;
	// 保留协议前缀，内部采样诊断最多 1023 TCHAR；外部 FString 输出仍归调用者。
	mutable TCHAR PlanSampleError[1024] = {};
	TSharedPtr<void, ESPMode::ThreadSafe> MissingHydrologyContext;
	void (*MissingHydrologyObserver)(void*, const FVoxelHydrologyRegionKey&) = nullptr;
};
