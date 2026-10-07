#pragma once

#include "CoreMinimal.h"
#include "Tasks/Task.h"
#include "Voxel/Task/VoxelTaskFairSelection.h"
#include "Voxel/Task/VoxelTerrainWorkSelection.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/Rendering/VoxelViewTypes.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"
#include "Voxel/Runtime/VoxelBlockState.h"
#include "Voxel/Rendering/VoxelTerrainStatistics.h"

struct FVoxelSectionCollisionResult;
struct FVoxelSectionMeshResult;
struct FVoxelVoxelProxyData;
struct FVoxelSurfaceTileData;
struct FVoxelWaterSurfaceTileData;
struct FVoxelMacroTileData;
struct FVoxelPersistentSection;
struct FVoxelDetailPlan;
struct FVoxelFineOverlayReadResult;
class FVoxelTerrainFrameContext;

enum class EVoxelWorkClass : uint8
{
	None = 0,
	Critical,
	Warmup,
	ExactData,
	Interactive,
	Visible,
	Boundary,
	Exploration,
	Background,
	Prefetch
};

enum class EVoxelTaskKind : uint8
{
	None = 0,
	GenerateExactBase,
	BuildCollision,
	BuildFineMesh,
	BuildVoxelProxy,
	BuildSurface,
	BuildWater,
	BuildMacro,
	BuildViewTransition,
	BuildVolumeTransition,
	BuildViewCoverage,
	BuildDetails,
	DecodeOverlay,
	EncodeRegion,
	NetworkRepresentation,
	BuildMapTile,

	// 项目层允许复用 Voxel Scheduler 的纯后台任务。
	// 不参与 Voxel 内部 ApplyTask switch。
	ProjectBackground,
	BuildInterest,
	BuildGenerationPlan,
	GenerateVoxelProxy,
	GenerateSurface,
	GenerateMacro,
	BuildNavigation,
	BuildFineShard
};

struct WHFRAMEWORK_API FVoxelTaskStamp
{
	FGuid OwnerId;
	uint64 OwnerIncarnation = 0;
	uint64 WorldEpoch = 0;
	uint64 Token = 0;
	uint64 Revision = 0;
	uint64 GeometryRevision = 0;
	bool bHasGeometryIdentity = false;
	FVoxelTerrainGeometry GeometryIdentity;
	FIntVector Section = FIntVector::ZeroValue;
	FVoxelViewKey ViewKey;
	FVoxelSurfaceTileKey SurfaceKey;
	FVoxelMacroTileKey MacroKey;
	uint64 RegistryRevision = 0;
	uint64 ShapeRevision = 0;

	bool operator==(const FVoxelTaskStamp& InOther) const;
};

FORCEINLINE uint32 GetTypeHash(const FVoxelTaskStamp& InStamp)
{
	uint32 Hash = ::GetTypeHash(InStamp.OwnerId);
	Hash = HashCombineFast(Hash, ::GetTypeHash(InStamp.OwnerIncarnation));
	Hash = HashCombineFast(Hash, ::GetTypeHash(InStamp.WorldEpoch));
	Hash = HashCombineFast(Hash, ::GetTypeHash(InStamp.Token));
	Hash = HashCombineFast(Hash, ::GetTypeHash(InStamp.Revision));
	Hash = HashCombineFast(Hash, ::GetTypeHash(InStamp.GeometryRevision));
	Hash = HashCombineFast(Hash, ::GetTypeHash(InStamp.bHasGeometryIdentity));
	if (InStamp.bHasGeometryIdentity)
	{
		Hash = HashCombineFast(Hash, ::GetTypeHash(InStamp.GeometryIdentity));
	}
	Hash = HashCombineFast(Hash, ::GetTypeHash(InStamp.Section));
	Hash = HashCombineFast(Hash, ::GetTypeHash(InStamp.ViewKey));
	Hash = HashCombineFast(Hash, ::GetTypeHash(InStamp.SurfaceKey));
	Hash = HashCombineFast(Hash, ::GetTypeHash(InStamp.MacroKey));
	Hash = HashCombineFast(Hash, ::GetTypeHash(InStamp.RegistryRevision));
	Hash = HashCombineFast(Hash, ::GetTypeHash(InStamp.ShapeRevision));
	return Hash;
}

struct WHFRAMEWORK_API FVoxelTaskKey
{
	EVoxelTaskKind Kind = EVoxelTaskKind::None;
	FVoxelTaskStamp Stamp;

	bool operator==(const FVoxelTaskKey& InOther) const
	{
		return Kind == InOther.Kind && Stamp == InOther.Stamp;
	}
};

FORCEINLINE uint32 GetTypeHash(const FVoxelTaskKey& InKey)
{
	return HashCombineFast(
		::GetTypeHash(static_cast<uint8>(InKey.Kind)),
		::GetTypeHash(InKey.Stamp));
}

/**
 * 项目层异步任务的通用纯 C++ Payload。
 * Payload 不能持有只能在 GameThread 访问的 UObject。
 */
struct WHFRAMEWORK_API FVoxelTaskCustomPayload
{
	mutable TSharedPtr<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe> WorkReceipt;
	virtual ~FVoxelTaskCustomPayload() = default;

	virtual uint64 GetAllocatedBytes() const
	{
		return sizeof(FVoxelTaskCustomPayload);
	}
	virtual uint64 GetResourceAccountedBytes() const
	{
		return 0;
	}
};

struct WHFRAMEWORK_API FVoxelTaskResult
{
	FVoxelTaskResult() = default;
	FVoxelTaskResult(const FVoxelTaskResult&) = delete;
	FVoxelTaskResult& operator=(const FVoxelTaskResult&) = delete;
	FVoxelTaskResult(FVoxelTaskResult&&) = default;
	FVoxelTaskResult& operator=(FVoxelTaskResult&&) = default;

	EVoxelTaskKind Kind = EVoxelTaskKind::None;
	FVoxelTaskStamp Stamp;

	bool bSuccess = false;
	bool bCanceled = false;
	bool bKnownEmptyMesh = false;
	bool bContinue = false;
	FString Error;

	// diagnostics
	double QueueMilliseconds = 0.0;
	double ExecuteMilliseconds = 0.0;
	double ApplyMilliseconds = 0.0;

	TArray<FVoxelBlockState> BaseBlocks;
	TArray<uint8> Payload;

	TSharedPtr<FVoxelSectionCollisionResult, ESPMode::ThreadSafe> Collision;
	TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe> FineMesh;
	TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe> VoxelProxyMesh;
	TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe> VolumeBoundaryMesh;
	TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe> SurfaceMesh;
	TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe> WaterMesh;
	TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe> MacroMesh;

	TSharedPtr<FVoxelVoxelProxyData, ESPMode::ThreadSafe> VoxelProxy;
	TSharedPtr<FVoxelSurfaceTileData, ESPMode::ThreadSafe> Surface;
	TSharedPtr<FVoxelWaterSurfaceTileData, ESPMode::ThreadSafe> Water;
	TSharedPtr<FVoxelMacroTileData, ESPMode::ThreadSafe> Macro;
	TSharedPtr<FVoxelDetailPlan, ESPMode::ThreadSafe> Details;
	TSharedPtr<FVoxelPersistentSection, ESPMode::ThreadSafe> PersistentSection;
	TSharedPtr<const FVoxelFineOverlayReadResult, ESPMode::ThreadSafe> OverlayReadResult;

	TSharedPtr<const FVoxelTaskCustomPayload, ESPMode::ThreadSafe> CustomPayload;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputResourceLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> OutputResourceLease;
	TSharedPtr<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe> WorkReceipt;
	void MarkWorkUsed() const { if (WorkReceipt) WorkReceipt->Resolve(true); }

	uint64 ResultBytes() const;
	uint64 GetResourceAccountedBytes() const;
	bool HasHeavyApply() const;
};

struct WHFRAMEWORK_API FVoxelTaskRequest
{
	TSharedPtr<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe> WorkReceipt;
	FVoxelTaskStamp Stamp;
	EVoxelTaskKind Kind = EVoxelTaskKind::None;
	EVoxelWorkClass WorkClass = EVoxelWorkClass::None;
	int32 SourcePriority = 0;
	int32 TerrainStage = INDEX_NONE;
	// 已接纳的局部发布准备/裁剪，持续数据压力下仍须有界完成。
	bool bPublicationContinuation = false;
	bool bTerrainDataOnly = false;

	double DistanceScore = 0.0;
	double ForwardScore = 0.0;
	FGuid SourceStableKey;
	EVoxelTerrainServiceBand ServiceBand = EVoxelTerrainServiceBand::None;
	double FirstRunnableSeconds = -1.0;
	double QueuedAt = 0.0;
	uint64 Sequence = 0;
	EVoxelTaskBlockReason Blocked = EVoxelTaskBlockReason::None;

	uint64 ReservedBytes = 0;
	uint64 InputBytes = 0;

	double AccumulatedExecuteMilliseconds = 0.0;
	bool bResumable = false;
	TFunction<EVoxelTaskBlockReason()> GetBlockReason;
	TFunction<FVoxelTaskResult(const TAtomic<bool>&)> Execute;

	/**
	 * 非空时由任务自己消费结果。
	 * 为空时回到 UVoxelModule::ApplyTask。
	 */
	TFunction<void(FVoxelTaskResult&&)> Apply;
};

struct WHFRAMEWORK_API FVoxelTaskBudget
{
	int32 MaxConcurrentTasks = 2;
	int32 CriticalReservedTasks = 1;
	int32 MaxPendingTasks = 256;
	uint64 MaxReservedBytes = 128ull * 1024ull * 1024ull;
	uint64 MaxInputBytes = 32ull * 1024ull * 1024ull;
	int32 MaxCompletedResultsPerFrame = 8;
	int32 MaxHeavyCompletedResultsPerFrame = 1;

	int32 MaxFineApplyPerFrame = 8;
	int32 MaxVoxelLODApplyPerFrame = 12;
	int32 MaxSurfaceApplyPerFrame = 16;
	int32 MaxMacroApplyPerFrame = 24;

	int32 MaxConcurrentSurfaceTasks = 1;
	int32 MaxConcurrentMacroTasks = 1;
	int32 MaxConcurrentCoarseTerrainTasks = 1;
	int32 MaxPendingCoarseTerrainTasks = 24;
	int32 MaxWaitingDependencyTasks = 0;
};

struct WHFRAMEWORK_API FVoxelTaskKindDiagnostics
{
	uint64 Completed = 0;
	uint64 Failed = 0;
	uint64 Canceled = 0;

	double TotalQueueMilliseconds = 0.0;
	double TotalExecuteMilliseconds = 0.0;
	double TotalApplyMilliseconds = 0.0;

	double MaximumQueueMilliseconds = 0.0;
	double MaximumExecuteMilliseconds = 0.0;
	double MaximumApplyMilliseconds = 0.0;
};

struct WHFRAMEWORK_API FVoxelTaskDiagnostics
{
	int32 Pending = 0;
	int32 Running = 0;
	int32 Critical = 0;
	int32 CriticalPending = 0;
	int32 CriticalRunning = 0;
	uint64 CriticalAdmissionDeferrals = 0;
	double LastAdmissionMilliseconds = 0.0;
	double LastResultApplyMilliseconds = 0.0;

	uint64 ReservedBytes = 0;
	uint64 QueuedInputBytes = 0;

	TMap<EVoxelTaskKind, FVoxelTaskKindDiagnostics> ByKind;
	TMap<EVoxelTaskKind, int32> PendingByKind;
	TMap<EVoxelTaskKind, int32> RunningByKind;
	TMap<EVoxelTaskKind, TMap<EVoxelTaskBlockReason, int32>> BlockedByKind;
	uint64 DuplicateRequests = 0;
	uint64 OversizedRequests = 0;
	uint64 CapacityDeferrals = 0;
	TMap<EVoxelTaskKind, TMap<EVoxelTaskBlockReason, int32>> AdmissionBlockedByKind;
	double OldestAdmissionWaitSeconds = 0.0;
};

class WHFRAMEWORK_API FVoxelTaskScheduler
{
public:
	FVoxelTaskScheduler();
	~FVoxelTaskScheduler();

	FVoxelTaskScheduler(const FVoxelTaskScheduler&) = delete;
	FVoxelTaskScheduler& operator=(const FVoxelTaskScheduler&) = delete;

	bool Enqueue(FVoxelTaskRequest&& InRequest);
	void ObserveDemand(EVoxelTaskKind InKind, const FVoxelTaskStamp& InStamp,
		EVoxelTaskBlockReason InReason, uint64 InInputBytes = 0, uint64 InResultBytes = 0);
	bool IsTerminal(const FVoxelTaskKey& InKey) const;
	void UpdatePriorities(TFunctionRef<void(EVoxelTaskKind, const FVoxelTaskStamp&,
		EVoxelWorkClass&, int32&, double&, double&)> InUpdate);
	void UpdateServicePriorities(TFunctionRef<void(EVoxelTaskKind, const FVoxelTaskStamp&,
		EVoxelWorkClass&, FVoxelTerrainServicePriority&)> InUpdate);
	void UpdateTerrainStages(TFunctionRef<void(EVoxelTaskKind, const FVoxelTaskStamp&, int32&)> InUpdate);

	void Tick(
		TFunctionRef<void(FVoxelTaskResult&&)> InApply,
		double InMaxApplyMilliseconds = 2.0);

	void CancelSection(const FIntVector& InSection);
	void CancelMatching(
		TFunctionRef<bool(EVoxelTaskKind, const FVoxelTaskStamp&)> InPredicate);
	void StopAndJoin();

	bool Has(
		const FVoxelTaskStamp& InStamp,
		EVoxelTaskKind InKind) const;

	bool HasSectionTask(const FIntVector& InSection) const;
	void SetSectionIdleCallback(TFunction<void(const FIntVector&)> InCallback);

	int32 ActiveCount() const;
	int32 CriticalCount() const;

	void SetBudget(const FVoxelTaskBudget& InBudget);
	void BindFrameContext(const FVoxelTerrainFrameContext& InContext);
	void SetObservedTerrainStage(int32 InStage);
	int32 GetObservedTerrainStage() const { return ObservedTerrainStage; }

	FVoxelTaskDiagnostics GetDiagnostics() const;
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> GetTerrainResources() const;
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> GetRuntimeResources() const;
	bool InitializeStatistics(uint64 InWorldEpoch);
	TSharedPtr<FVoxelTerrainStatistics, ESPMode::ThreadSafe> GetTerrainStatistics() const { return Statistics; }
	static bool IsHigherPriority(
		const FVoxelTaskRequest& InA,
		const FVoxelTaskRequest& InB);
	static FVoxelTerrainServicePriority ResolveServicePriority(const FVoxelTaskRequest& InRequest);

private:
	struct FSlot
	{
		TSharedPtr<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe> WorkReceipt;
		TAtomic<bool> Cancel { false };
		FVoxelTaskResult Result;
		double ExecuteMicroseconds = 0.0;
	};

	struct FRunning
	{
		FVoxelTaskStamp Stamp;
		EVoxelTaskKind Kind = EVoxelTaskKind::None;
		EVoxelWorkClass WorkClass = EVoxelWorkClass::None;
		int32 SourcePriority = 0;
		int32 TerrainStage = INDEX_NONE;
		bool bPublicationContinuation = false;
		bool bTerrainDataOnly = false;

		uint64 Sequence = 0;
		double QueuedAt = 0.0;
		double DistanceScore = 0.0;
		double ForwardScore = 0.0;
		FGuid SourceStableKey;
		EVoxelTerrainServiceBand ServiceBand = EVoxelTerrainServiceBand::None;
		uint64 ServiceChargeSerial = 0;
		bool bServiceNear = false;
		bool bServiceSettled = false;
		uint64 ReservedBytes = 0;

		TSharedPtr<FSlot, ESPMode::ThreadSafe> Slot;
		UE::Tasks::FTask Task;
		TFunction<void(FVoxelTaskResult&&)> Apply;
		TUniquePtr<FVoxelTaskRequest> Resume;
	};

	struct FCompleted
	{
		FVoxelTaskResult Result;
		EVoxelWorkClass WorkClass = EVoxelWorkClass::None;
		int32 TerrainStage = INDEX_NONE;
		TFunction<void(FVoxelTaskResult&&)> Apply;
	};

	static bool UsesSectionKey(EVoxelTaskKind InKind);
	int32 RunningCount(EVoxelTaskKind InKind) const;
	bool CanStartKind(EVoxelTaskKind InKind) const;

	void Pump();
	void QueueCanceled(FVoxelTaskRequest&& InRequest);

	void AddActive(
		const FVoxelTaskStamp& InStamp,
		EVoxelTaskKind InKind,
		EVoxelWorkClass InWorkClass);

	void RemoveActive(
		const FVoxelTaskStamp& InStamp,
		EVoxelTaskKind InKind,
		EVoxelWorkClass InWorkClass);

	void RecordCompletedResult(const FVoxelTaskResult& InResult);

private:
	TArray<FVoxelTaskRequest> Pending;
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> TerrainResources =
		MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> RuntimeResources =
		MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	TArray<FRunning> Running;
	TArray<FCompleted> Canceled;

	TSet<FVoxelTaskKey> ActiveKeys;
	TMap<FIntVector, int32> SectionTaskCounts;
	TFunction<void(const FIntVector&)> SectionIdleCallback;

	FVoxelTaskBudget Budget;
	const FVoxelTerrainFrameContext* FrameContext = nullptr;
	FVoxelTaskDiagnostics Diagnostics;
	TSharedPtr<FVoxelTerrainStatistics, ESPMode::ThreadSafe> Statistics;
	FGuid StatisticsOwnerId = FGuid::NewGuid();
	uint64 StatisticsWorldEpoch = 0;
	struct FAdmissionAttempt
	{
		double FirstRequested = 0.0;
		uint64 Sequence = 0;
		EVoxelTaskBlockReason Blocked = EVoxelTaskBlockReason::None;
		uint64 InputBytes = 0;
		uint64 ResultBytes = 0;
		uint64 Rejections = 0;
	};
	TMap<FVoxelTaskKey, FAdmissionAttempt> AdmissionAttempts;
	FVoxelTerrainWorkSelection WorkerSelection;
	FVoxelTerrainWorkSelection CompletedSelection;
	TMap<EVoxelTaskKind, double> WorkerCostEstimates;
	TMap<EVoxelTaskKind, double> ApplyCostEstimates;
	uint64 NextSequence = 0;

	double LastBudgetWarning = -10.0;
	bool bStopped = false;
	int32 ObservedTerrainStage = 3;
	uint64 ReservedBytes = 0;
	uint64 QueuedInputBytes = 0;
	uint64 RunningContinuationInputBytes = 0;
	int32 CriticalTaskCount = 0;
	uint64 CriticalAdmissionDeferrals = 0;
};
