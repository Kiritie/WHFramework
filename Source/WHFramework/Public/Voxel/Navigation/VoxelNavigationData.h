#pragma once

#include "NavMesh/RecastNavMesh.h"
#include "HAL/CriticalSection.h"
#include "Voxel/Navigation/VoxelNavigationTypes.h"
#include "VoxelNavigationData.generated.h"

class UVoxelModule;
class FVoxelRecastTileGenerator;
class FVoxelRecastNavMeshGenerator;
struct FVoxelNavigationTileCapture;
class FVoxelTerrainFrameContext;
class FVoxelNavigationNativeQueryScope;
class FVoxelNavigationGuardFilter;
struct FVoxelNavigationQueryInputs;
struct FVoxelNavigationAsyncQueryTicket;
struct FVoxelNavigationRepathControl;
class UVoxelNavigationSystem;

/** 将实际 Recast tile 提交与冻结的体素输入绑定。 */
UCLASS(Config=Engine)
class WHFRAMEWORK_API AVoxelNavigationData : public ARecastNavMesh
{
	GENERATED_BODY()

public:
	AVoxelNavigationData(const FObjectInitializer& InObjectInitializer);
	bool BindTerrain(UVoxelModule& InModule, TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources);
	void UnbindTerrain(const UVoxelModule& InModule);
	bool HasCurrentTileForNode(NavNodeRef InNode, const FVoxelNavigationInputVersion& InVersion) const;
	void RetryBlockedTiles(FVoxelTerrainFrameContext& InFrame);
	bool TryCloseQueriesForWorldStop();
	void ResumeQueriesAfterWorldStopWait();
	void RetirePathInputs(
		FVoxelTerrainFrameContext& InFrame);
	void AdvancePathMaintenance(
		FVoxelTerrainFrameContext& InFrame);
	virtual void TickActor(
		float InDeltaSeconds,
		ELevelTick InTickType,
		FActorTickFunction& InTickFunction) override;
	virtual void RebuildAll() override;
	virtual void ConditionalConstructGenerator() override;
	virtual void TickAsyncBuild(
		float InDeltaSeconds) override;
	bool PrepareAsyncQuery(
		FAsyncPathFindingQuery& InQuery);
	ENavigationQueryResult::Type ValidateAsyncResult(
		const FAsyncPathFindingQuery& InQuery) const;
	void ReleaseAsyncQuery(
		const FAsyncPathFindingQuery& InQuery);
#if WITH_RECAST
	virtual void OnNavMeshTilesUpdated(const TArray<FNavTileRef>& InChangedTiles) override;
	virtual void UpdateActiveTiles(
		const TArray<FNavigationInvokerRaw>& InInvokers) override;
	virtual void RemoveTiles(
		const TArray<FIntPoint>& InTiles) override;
#endif
	virtual void BeginDestroy() override;
	virtual void CleanUp() override;
	virtual bool IsReadyForFinishDestroy() override;

protected:
#if WITH_RECAST
	virtual FRecastNavMeshGenerator* CreateGeneratorInstance() override;
#endif

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FVoxelNavigationNativeTileTest;
#endif
	friend class FVoxelRecastTileGenerator;
	friend class FVoxelRecastNavMeshGenerator;
	friend class FVoxelNavigationNativeQueryScope;
	friend class FVoxelNavigationGuardFilter;
	friend struct FVoxelNavigationQueryInputs;
	friend struct FVoxelNavigationRepathControl;
	friend class UVoxelNavigationSystem;
	TSharedPtr<FVoxelNavigationAsyncQueryTicket, ESPMode::ThreadSafe> FindAsyncTicket(
		const FSharedConstNavQueryFilter& InFilter) const;
	void RetireAsyncTickets(
		FVoxelTerrainFrameContext& InFrame);
	bool CanCleanupNativeTerrain() const;
	bool AdmitRepath(
		const FNavPathSharedPtr& InPath,
		ENavPathUpdateType::Type InReason);
	void CompleteRepath(
		FVoxelNavigationRepathControl& InControl,
		uint32 InQueryId,
		ENavigationQueryResult::Type InResult,
		FNavPathSharedPtr InPath);
	bool QueueWaitingRepath(
		const FNavPathSharedPtr& InPath,
		ENavPathUpdateType::Type InReason);
	void RemoveRepathControl(
		FVoxelNavigationRepathControl& InControl);
	void FinalizeRepath(
		FVoxelNavigationRepathControl& InControl);
	const FVoxelNavigationTileCapture* FindUsableNodeCapture(
		NavNodeRef InNode) const;
	TSharedPtr<FVoxelNavigationQueryInputs, ESPMode::ThreadSafe> CaptureQueryInputs() const;
	TSharedPtr<FVoxelNavigationQueryInputs, ESPMode::ThreadSafe> FindPathInputs(
		const FNavPathSharedPtr& InPath) const;
	bool ReservePathInputSlot(
		const FNavPathSharedPtr& InExisting,
		const TSharedPtr<FVoxelNavigationQueryInputs, ESPMode::ThreadSafe>& InExpectedPrevious) const;
	void ReleasePathInputSlot(
		const FNavPathSharedPtr& InExisting,
		bool bInRegistered) const;
	bool RegisterPathInputs(
		const FNavPathSharedPtr& InPath,
		TSharedRef<FVoxelNavigationQueryInputs, ESPMode::ThreadSafe> InInputs) const;
	void RevokeAllPathInputs(
		bool bInAllowRepath = true,
		bool bInRetainPathDemand = false);
	static FPathFindingResult FindTerrainPath(
		const FNavAgentProperties& InAgent,
		const FPathFindingQuery& InQuery);
	static FPathFindingResult FindTerrainPathWithWait(
		const FNavAgentProperties& InAgent,
		const FPathFindingQuery& InQuery,
		bool* OutWaiting);
	bool IsPathInputAttemptCurrent(
		const FNavPathSharedPtr& InPath,
		const TSharedPtr<FVoxelNavigationQueryInputs, ESPMode::ThreadSafe>& InInputs) const;
	static bool TestTerrainPath(
		const FNavAgentProperties& InAgent,
		const FPathFindingQuery& InQuery,
		int32* OutVisited);
	static bool TestTerrainHierarchicalPath(
		const FNavAgentProperties& InAgent,
		const FPathFindingQuery& InQuery,
		int32* OutVisited);
	bool BeginTerrainQuery() const;
	void FinishTerrainQuery() const;
	bool TryBeginNativeWrite();
	void FinishNativeWrite();
	bool HasDeferredNativeWrite() const;
	bool StageGeneratedTile(const TSharedRef<FVoxelNavigationTileCapture, ESPMode::ThreadSafe>& InCapture);
	bool IsCaptureCurrent(const FVoxelNavigationTileCapture& InCapture) const;
	void ClearTerrainEvidence();
	void InvalidateNativeEvidence();
	void HoldBlockedTile(const FIntPoint& InCoordinate, const FBox& InBounds);
	struct FCaptureWait
	{
		FIntPoint Coordinate;
		FBox Bounds;
		uint64 ReleaseVersion = 0;
		bool operator==(const FCaptureWait& InOther) const { return Coordinate == InOther.Coordinate; }
		friend uint32 GetTypeHash(const FCaptureWait& InWait) { return GetTypeHash(InWait.Coordinate); }
	};
	TWeakObjectPtr<UVoxelModule> TerrainModule;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> StorageLease;
	struct FCommittedTile
	{
		FNavTileRef Reference{0};
		TSharedPtr<const FVoxelNavigationTileCapture, ESPMode::ThreadSafe> Capture;
	};
	// Native slot 的数量固定；salt 换代直接替换该槽，不累积历史 Ref 目录。
	TMap<int32, FCommittedTile> CommittedTiles;
	TMap<FNavTileRef, TSharedPtr<const FVoxelNavigationTileCapture, ESPMode::ThreadSafe>> PendingTiles;
	TSet<FCaptureWait> BlockedTiles;
	// 路径对象由引擎/调用者拥有；此目录只保留其实际输入，死亡弱引用按共享帧预算退出。
	mutable FCriticalSection PathInputsLock;
	mutable TMap<const FNavigationPath*, TSharedPtr<FVoxelNavigationQueryInputs, ESPMode::ThreadSafe>> PathInputs;
	mutable TSet<const FNavigationPath*> InFlightPaths;
	// 每个请求使用独立的外层filter地址；实际worker把完成时的输入尝试写回该记录。
	mutable FCriticalSection AsyncTicketsLock;
	TMap<const FNavigationQueryFilter*, TSharedPtr<FVoxelNavigationAsyncQueryTicket, ESPMode::ThreadSafe>> AsyncTickets;
	// GUID只索引实际生产者；原引擎QueryID仍用于对外请求/完成/Abort合同。
	TMap<FGuid, TSharedPtr<FVoxelNavigationRepathControl, ESPMode::ThreadSafe>> PendingRepaths;
	TMap<const FNavigationPath*, FGuid> PendingRepathPaths;
	int32 AsyncTicketCursor = 0;
	double AsyncTicketRetirementCostMilliseconds = 0.005;
	FGuid TerrainBindingId;
	mutable int32 ReservedPathInputSlots = 0;
	int32 PathInputCursor = 0;
	double PathRetirementCostMilliseconds = 0.005;
	int32 ActivePathCursor = 0;
	int32 ObservedPathCursor = 0;
	int32 ObservedPathsRemaining = 0;
	int32 RepathCursor = 0;
	int32 RepathReturnCursor = 0;
	uint8 PathMaintenancePhase = 0;
	double LastPathObservationWorldTime = -1.0;
	double PathMaintenanceCostMilliseconds = 0.005;
	uint64 PathMaintenanceFrame = MAX_uint64;
	// 真实 Detour 查询的读锁与GT原生写入互斥；GT只TryLock，不等待查询线程。
	mutable FRWLock NativeAccess;
	mutable TAtomic<int32> NativeQueryReaders{0};
	TAtomic<bool> bQueryAdmissionOpen{false};
	int32 NativeWriteDepth = 0;
	bool bNativeRebuildPending = false;
	bool bNativeGeneratorRebuildPending = false;
	bool bNativeBoundsPending = false;
	bool bNativeCancelPending = false;
	bool bNativeCleanupPending = false;
	bool bNativeInvokersPending = false;
	bool bWorldCloseRequested = false;
	bool bTerrainClearPending = false;
	TSet<FIntPoint> DeferredRemovals;
	TArray<FIntPoint> RemovalScratch;
	TArray<FNavTileRef> RemovalReferenceScratch;
	FGuid EvidenceOwnerId = FGuid::NewGuid();
	uint64 WorldEpoch = 0;
	int32 EvidenceCapacity = 0;
	int32 BlockedCursor = 0;
	double RetryCostMilliseconds = 0.005;
};
