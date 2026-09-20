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
	NetworkRepresentation
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
	void StopAndJoin();
	bool Has(const FVoxelTaskStamp& InStamp, EVoxelTaskKind InKind) const;
	bool HasSectionTask(const FIntVector& InSection) const;
	int32 ActiveCount() const;
	int32 CriticalCount() const;
	void SetBudget(const FVoxelTaskBudget& InBudget);

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

	static bool IsHigherPriority(
		const FVoxelTaskRequest& InA,
		const FVoxelTaskRequest& InB);
	static bool IsHeavyApplyKind(EVoxelTaskKind InKind);
	static bool UsesSectionKey(EVoxelTaskKind InKind);

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

private:
	TArray<FVoxelTaskRequest> Pending;
	TArray<FRunning> Running;
	TArray<FCompleted> Canceled;
	TSet<FVoxelTaskKey> ActiveKeys;
	TMap<FIntVector, int32> SectionTaskCounts;
	FVoxelTaskBudget Budget;
	bool bStopped = false;
	uint64 ReservedBytes = 0;
	uint64 QueuedInputBytes = 0;
	int32 CriticalTaskCount = 0;
};
