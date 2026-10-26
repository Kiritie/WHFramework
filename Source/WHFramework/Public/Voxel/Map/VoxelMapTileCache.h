#pragma once

#include "CoreMinimal.h"
#include "Scene/SceneModuleTypes.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "Voxel/Task/VoxelTaskScheduler.h"

class FVoxelEmergeManager;
class FVoxelGenerationPipeline;
class FVoxelTaskScheduler;
class UTexture2D;
struct FVoxelGenerationRuntimeConfig;
struct FVoxelTaskResult;
struct FVoxelColumnSample;

using FVoxelMapColorResolver = TFunction<FColor(const FVoxelColumnSample&, const FVoxelGenerationRuntimeConfig&)>;

struct WHFRAMEWORK_API FVoxelMapTileKey
{
	FIntPoint Coordinate = FIntPoint::ZeroValue;
	int32 Step = 16;

	bool operator==(const FVoxelMapTileKey& InOther) const
	{
		return Coordinate == InOther.Coordinate && Step == InOther.Step;
	}
};

FORCEINLINE uint32 GetTypeHash(const FVoxelMapTileKey& InKey)
{
	return HashCombineFast(GetTypeHash(InKey.Coordinate), GetTypeHash(InKey.Step));
}

// 地图请求与等待由GT持有，后台批次只写原瓦片候选，完整后发布纹理。
class WHFRAMEWORK_API FVoxelMapTileCache
{
public:
	static constexpr int32 TileSide = 64;

	FVoxelMapTileCache(
		FVoxelTaskScheduler& InScheduler,
		FVoxelEmergeManager& InEmergeManager,
		TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator,
		TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		uint64 InWorldEpoch,
		double InBlockSize);
	~FVoxelMapTileCache();
	FVoxelMapTileCache(const FVoxelMapTileCache&) = delete;
	FVoxelMapTileCache& operator=(const FVoxelMapTileCache&) = delete;

	static int32 SelectStep(float InCellPixelSize);
	static void VisibleTiles(
		const FSceneMapView& InView,
		const FVector2D& InPanelSize,
		double InBlockSize,
		TArray<FVoxelMapTileKey>& OutKeys);

	void Request(const FGuid& InSource, const TArray<FVoxelMapTileKey>& InKeys, const FSceneMapView& InView);
	void ReleaseRequest(const FGuid& InSource);
	void Tick(double InDeadline);
	void OnGenerationDependencyReady(const FVoxelGenerationConsumerKey& InConsumer);
	void SetColorResolver(FVoxelMapColorResolver InResolver, uint32 InStyleRevision);
	static FVoxelMapTileKey ParentKey(const FVoxelMapTileKey& InKey, int32 InParentStep);
	static FBox2f ParentUV(const FVoxelMapTileKey& InKey, const FVoxelMapTileKey& InParent);
	bool FindDrawBrush(const FVoxelMapTileKey& InKey, FSlateBrush& OutBrush) const;
	const FSlateBrush* FindBrush(const FVoxelMapTileKey& InKey) const;
	uint64 GetRevision() const { return Revision; }
	int32 GetReadyTileCount() const { return Tiles.Num(); }
	int32 GetPendingTileCount() const { return Pending.Num(); }
	bool HasColorResolver() const { return bool(ColorResolver); }
	int32 GetViewCount() const { return Views.Num(); }

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FVoxelMapStyleEpochTest;
	friend class FVoxelMapCacheLifetimeTest;
#endif
	struct FBuildWork;
	struct FViewRequest
	{
		TArray<FVoxelMapTileKey> Keys;
		FSceneMapView View;
		uint64 LastRequestFrame = 0;
	};
	struct FPendingTile
	{
		FVoxelGenerationConsumerKey Consumer;
		FVoxelTaskStamp Stamp;
		TSharedPtr<FBuildWork, ESPMode::ThreadSafe> Work;
		TOptional<FVoxelTaskRequest> Request;
		TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> AdmissionResources;
		uint64 CapacityVersion = 0;
		double DistanceScore = 0.0;
		int32 PreflightStartRow = INDEX_NONE;
		int32 PreflightRows = 0;
		bool bSubmitted = false;
		bool bFailed = false;
		bool bWakePending = true;
		bool bWaitingCapacity = false;
	};

	struct FTile
	{
		TStrongObjectPtr<UTexture2D> Texture;
		FSlateBrush Brush;
		uint64 LastUse = 0;
	};

	void ApplyTile(const FVoxelMapTileKey& InKey, FVoxelTaskResult&& InResult);
	void RefreshRequests();
	bool AdvanceRequest(const FVoxelMapTileKey& InKey, double InDistanceScore);
	bool DeferRequest(const FVoxelMapTileKey& InKey, const FVoxelGenerationConsumerKey& InConsumer,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources, uint64 InBeforeAdmission,
		const FString& InError);
	void CancelRequest(const FVoxelMapTileKey& InKey);
	void ResetRequests();
	bool ReservePendingStorage(int32 InRequired, FString& OutError);
	int32 SelectBatchRows(const FVoxelMapTileKey& InKey, const FIntPoint& InOrigin, int32 InStartRow) const;
	void EvictOldest();

	FVoxelTaskScheduler& Scheduler;
	FVoxelEmergeManager& EmergeManager;
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator;
	TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	uint64 WorldEpoch = 0;
	FGuid GenerationOwnerId = FGuid::NewGuid();
	double BlockSize = 1.0;
	uint64 Sequence = 0;
	uint64 Revision = 0;
	uint32 StyleRevision = 0;
	uint64 StyleEpoch = 0;
	bool bResetting = false;
	FVoxelMapColorResolver ColorResolver;
	TMap<FVoxelMapTileKey, FTile> Tiles;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PendingStorageLease;
	int32 PendingCapacity = 0;
	TMap<FVoxelMapTileKey, FPendingTile> Pending;
	TMap<FGuid, FViewRequest> Views;
};
