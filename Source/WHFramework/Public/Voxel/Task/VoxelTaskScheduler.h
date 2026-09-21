#pragma once

#include "CoreMinimal.h"
#include "Tasks/Task.h"
#include "Voxel/Rendering/VoxelViewTypes.h"
#include "Voxel/Runtime/VoxelBlockState.h"

struct FVoxelSectionCollisionResult;
struct FVoxelSectionMeshResult;
struct FVoxelVoxelProxyData;
struct FVoxelSurfaceTileData;
struct FVoxelWaterSurfaceTileData;
struct FVoxelMacroTileData;
struct FVoxelPersistentSection;
struct FVoxelDetailPlan;

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
	Background
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
	BuildDetails,
	DecodeOverlay,
	EncodeRegion,
	NetworkRepresentation,

	// 项目层允许复用 Voxel Scheduler 的纯后台任务。
	// 不参与 Voxel 内部 ApplyTask switch。
	ProjectBackground
};

struct WHFRAMEWORK_API FVoxelTaskStamp
{
	uint64 WorldEpoch = 0;
	uint64 Token = 0;
	uint64 Revision = 0;
	FIntVector Section = FIntVector::ZeroValue;
	FVoxelViewKey ViewKey;
	FVoxelSurfaceTileKey SurfaceKey;
	FVoxelMacroTileKey MacroKey;

	bool operator==(const FVoxelTaskStamp& InOther) const;
};

FORCEINLINE uint32 GetTypeHash(const FVoxelTaskStamp& InStamp)
{
	uint32 Hash = ::GetTypeHash(InStamp.WorldEpoch);
	Hash = HashCombineFast(Hash, ::GetTypeHash(InStamp.Token));
	Hash = HashCombineFast(Hash, ::GetTypeHash(InStamp.Revision));
	Hash = HashCombineFast(Hash, ::GetTypeHash(InStamp.Section));
	Hash = HashCombineFast(Hash, ::GetTypeHash(InStamp.ViewKey));
	Hash = HashCombineFast(Hash, ::GetTypeHash(InStamp.SurfaceKey));
	Hash = HashCombineFast(Hash, ::GetTypeHash(InStamp.MacroKey));
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
	virtual ~FVoxelTaskCustomPayload() = default;

	virtual uint64 GetAllocatedBytes() const
	{
		return sizeof(FVoxelTaskCustomPayload);
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
	FString Error;

	// diagnostics
	double QueueMilliseconds = 0.0;
	double ExecuteMilliseconds = 0.0;
	double ApplyMilliseconds = 0.0;

	TArray<FVoxelBlockState> BaseBlocks;
	TArray<uint8> Payload;

	TSharedPtr<FVoxelSectionCollisionResult> Collision;
	TSharedPtr<FVoxelSectionMeshResult> FineMesh;
	TSharedPtr<FVoxelSectionMeshResult> VoxelProxyMesh;
	TSharedPtr<FVoxelSectionMeshResult> SurfaceMesh;
	TSharedPtr<FVoxelSectionMeshResult> WaterMesh;
	TSharedPtr<FVoxelSectionMeshResult> MacroMesh;

	TSharedPtr<FVoxelVoxelProxyData> VoxelProxy;
	TSharedPtr<FVoxelSurfaceTileData> Surface;
	TSharedPtr<FVoxelWaterSurfaceTileData> Water;
	TSharedPtr<FVoxelMacroTileData> Macro;
	TSharedPtr<FVoxelDetailPlan, ESPMode::ThreadSafe> Details;
	TSharedPtr<FVoxelPersistentSection> PersistentSection;

	TSharedPtr<const FVoxelTaskCustomPayload, ESPMode::ThreadSafe> CustomPayload;

	uint64 ResultBytes() const;
};

struct WHFRAMEWORK_API FVoxelTaskRequest
{
	FVoxelTaskStamp Stamp;
	EVoxelTaskKind Kind = EVoxelTaskKind::None;
	EVoxelWorkClass WorkClass = EVoxelWorkClass::None;

	double DistanceScore = 0.0;
	double ForwardScore = 0.0;
	double QueuedAt = 0.0;

	uint64 ReservedBytes = 0;
	uint64 InputBytes = 0;

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
	int32 MaxPendingTasks = 256;
	uint64 MaxReservedBytes = 128ull * 1024ull * 1024ull;
	uint64 MaxInputBytes = 32ull * 1024ull * 1024ull;
	int32 MaxCompletedResultsPerFrame = 8;
	int32 MaxHeavyCompletedResultsPerFrame = 1;
	int32 MaxConcurrentSurfaceTasks = 1;
	int32 MaxConcurrentMacroTasks = 1;
	int32 MaxConcurrentCoarseTerrainTasks = 1;
	int32 MaxPendingCoarseTerrainTasks = 24;
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

	uint64 ReservedBytes = 0;
	uint64 QueuedInputBytes = 0;

	TMap<EVoxelTaskKind, FVoxelTaskKindDiagnostics> ByKind;
	TMap<EVoxelTaskKind, int32> PendingByKind;
	TMap<EVoxelTaskKind, int32> RunningByKind;
};

class WHFRAMEWORK_API FVoxelTaskScheduler
{
public:
	FVoxelTaskScheduler();
	~FVoxelTaskScheduler();

	FVoxelTaskScheduler(const FVoxelTaskScheduler&) = delete;
	FVoxelTaskScheduler& operator=(const FVoxelTaskScheduler&) = delete;

	bool Enqueue(FVoxelTaskRequest&& InRequest);

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

	int32 ActiveCount() const;
	int32 CriticalCount() const;

	void SetBudget(const FVoxelTaskBudget& InBudget);

	FVoxelTaskDiagnostics GetDiagnostics() const;
	static bool IsHigherPriority(
		const FVoxelTaskRequest& InA,
		const FVoxelTaskRequest& InB);

private:
	struct FSlot
	{
		TAtomic<bool> Cancel { false };
		FVoxelTaskResult Result;
	};

	struct FRunning
	{
		FVoxelTaskStamp Stamp;
		EVoxelTaskKind Kind = EVoxelTaskKind::None;
		EVoxelWorkClass WorkClass = EVoxelWorkClass::None;

		double QueuedAt = 0.0;
		uint64 ReservedBytes = 0;

		TSharedPtr<FSlot, ESPMode::ThreadSafe> Slot;
		UE::Tasks::FTask Task;
		TFunction<void(FVoxelTaskResult&&)> Apply;
	};

	struct FCompleted
	{
		FVoxelTaskResult Result;
		TFunction<void(FVoxelTaskResult&&)> Apply;
	};

	static bool IsHeavyApplyKind(EVoxelTaskKind InKind);
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
	TArray<FRunning> Running;
	TArray<FCompleted> Canceled;

	TSet<FVoxelTaskKey> ActiveKeys;
	TMap<FIntVector, int32> SectionTaskCounts;

	FVoxelTaskBudget Budget;
	FVoxelTaskDiagnostics Diagnostics;

	bool bStopped = false;
	uint64 ReservedBytes = 0;
	uint64 QueuedInputBytes = 0;
	int32 CriticalTaskCount = 0;
};
