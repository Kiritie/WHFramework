#pragma once

#include "CoreMinimal.h"
#include "Templates/Atomic.h"
#include "Templates/SharedPointer.h"
#include "Voxel/Runtime/VoxelBlockState.h"
#include "Voxel/Save/VoxelDeltaTypes.h"

class FVoxelTerrainResourceLease;
class UVoxelSceneRegion;
struct FVoxelTaskResult;
struct FVoxelSection;

enum class EVoxelSectionStatus : uint8
{
    None,
    Allocated,
    BaseReady,
    DataReady,
    Failed,
    Unloading
};

struct WHFRAMEWORK_API FVoxelSectionStamp
{
    uint64 Epoch = 0;
    uint64 Token = 0;
    uint64 RecipeHash = 0;

    bool operator==(
        const FVoxelSectionStamp& InOther) const
    {
        return Epoch ==
                InOther.Epoch &&
            Token ==
                InOther.Token &&
            RecipeHash ==
                InOther.RecipeHash;
    }
};

enum class EVoxelRuntimeRetirementState : uint8
{
	Active,
	Candidate,
	Waiting,
	ReadyToDetach,
	Detaching,
	ReadyToFree,
	Freed,
	Revived
};

enum class EVoxelRuntimeRetirementBlocker : uint8
{
	None = 0,
	Pin = 1 << 0,
	Task = 1 << 1,
	Save = 1 << 2,
	Replacement = 1 << 3,
	Grace = 1 << 4
};
ENUM_CLASS_FLAGS(EVoxelRuntimeRetirementBlocker)

struct WHFRAMEWORK_API FVoxelRuntimeSectionIdentity
{
	uint64 WorldEpoch = 0;
	uint64 Incarnation = 0;
	bool operator==(const FVoxelRuntimeSectionIdentity& InOther) const = default;
};

// 相同资源实例可被多次接纳；每次访问有独立句柄，旧释放不能命中新消费者。
struct WHFRAMEWORK_API FVoxelRuntimeSectionPin
{
	FVoxelRuntimeSectionIdentity Resource;
	FGuid ConsumerId;
	bool operator==(const FVoxelRuntimeSectionPin& InOther) const = default;
};

// 固定退休头随 Section MetadataLease 一次授予；等待和唤醒不再分配队列节点。
struct WHFRAMEWORK_API FVoxelRuntimeRetirementTicket
{
	FIntVector Section = FIntVector::ZeroValue;
	FVoxelSectionStamp Stamp;
	FVoxelRuntimeSectionIdentity Resource;
	uint64 Serial = 0;
	uint64 DueFrame = 0;
	EVoxelRuntimeRetirementState State = EVoxelRuntimeRetirementState::Active;
	EVoxelRuntimeRetirementBlocker Blockers = EVoxelRuntimeRetirementBlocker::None;
	FVoxelSection* ReadyPrevious = nullptr;
	FVoxelSection* ReadyNext = nullptr;
	int32 TimerIndex = INDEX_NONE;
	bool bQueued = false;
};

struct WHFRAMEWORK_API FVoxelSection
{
    TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
    TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> DataLease;
    TSharedPtr<FVoxelTaskResult, ESPMode::ThreadSafe> PendingPublish;
    uint64 PendingCapacityVersion = 0;
    FVoxelSectionStamp Stamp;
    uint64 Incarnation = 0;
    FVoxelRuntimeSectionIdentity GetResourceIdentity() const
    {
        return {Stamp.Epoch, Incarnation};
    }

	EVoxelSectionStatus Status =
		EVoxelSectionStatus::None;
	FString Error;

    /**
     * Immutable natural base。
     * 可被 Proxy/Compare/Restore Natural 复用。
     */
    TSharedPtr<
        const TArray<FVoxelBlockState>,
        ESPMode::ThreadSafe> BaseBlocks;

    /**
     * DataReady 后的最终 Exact Data。
     * 第一版保持完整 16^3 数组，便于兼容现有编辑/交互代码。
     */
    TArray<FVoxelBlockState> Blocks;

    /**
     * 只存相对 Natural Base 的差异。
     */
    TMap<int32, FVoxelBlockState> Overlay;

	TMap<int32, FVoxelBlockEntityState> Entities;

    uint64 CommittedRevision = 0;
	uint64 PersistedRevision = 0;

    int32 GetPinCount() const
    {
        return PinCount.Load();
    }
    FVoxelRuntimeRetirementTicket Retirement;
	// 局部协调票据随 Section 元数据授予；排队不持有第二份驻留状态或额外节点。
	FIntVector SimulationKey = FIntVector::ZeroValue;
	FVoxelRuntimeSectionIdentity SimulationIdentity;
	FVoxelSection* SimulationPrevious = nullptr;
	FVoxelSection* SimulationNext = nullptr;
	// 区域失败时保持原票据；显式恢复按局部链逐项唤醒，不重扫 SimulationSections。
	UVoxelSceneRegion* SimulationWaitingRegion = nullptr;
	FVoxelSection* SimulationFailurePrevious = nullptr;
	FVoxelSection* SimulationFailureNext = nullptr;
	// SaveUncommitted 与 Simulation 使用同一个驻留 owner；链节点属于已授予的 Section 元数据。
	UVoxelSceneRegion* SimulationSaveRegion = nullptr;
	FVoxelSection* SimulationSavePrevious = nullptr;
	FVoxelSection* SimulationSaveNext = nullptr;
	FGuid SimulationSaveRegionId;
	uint64 SimulationSaveRevision = 0;
	uint64 SimulationCapacityVersion = 0;
	uint64 SimulationGeometryCapacityVersion = 0;
	bool bSimulationQueued = false;
	bool bSimulationCapacityBlocked = false;

    uint64 LastWantedFrame = 0;
	int32 ResidencyOrderIndex = INDEX_NONE;

    bool bCollisionDirty = true;
    bool bFineMeshDirty = true;
    bool bSimulationWanted = false;
	// Module 的 SimulationSections 是状态 owner；此位只记录该 owner 已取得的稳定 Runtime pin。
	bool bSimulationPinned = false;

private:
	friend class FVoxelWorldRuntime;
	TAtomic<int32> PinCount { 0 };
};
