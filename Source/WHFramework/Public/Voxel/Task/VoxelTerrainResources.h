#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Voxel/Generation/VoxelGenerationConsumer.h"

enum class EVoxelTerrainResourcePool : uint8
{
	Input,
	Working,
	Staged,
	Resident,
	ResidentOverlap,
	Count
};

enum class EVoxelTerrainResourceAdmission : uint8
{
	None,
	Granted,
	CapacityBlocked,
	CapacityUnachievable,
	InvalidRequest
};

struct WHFRAMEWORK_API FVoxelTerrainResourceLimits
{
	uint64 InputBytes = 64ull * 1024 * 1024;
	uint64 WorkingBytes = 256ull * 1024 * 1024;
	uint64 StagedBytes = 128ull * 1024 * 1024;
	uint64 ResidentBytes = 512ull * 1024 * 1024;
	uint64 OverlapBytes = 128ull * 1024 * 1024;

	bool IsValid() const;
};

struct WHFRAMEWORK_API FVoxelTerrainResourceSnapshot
{
	uint64 Version = 0;
	uint64 ReleaseVersion = 0;
	uint64 LedgerMetadataBytes = 0;
	uint64 LeaseHandleBytes = 0;
	int32 LeaseCount = 0;
	int32 ReservedSplitCount = 0;
	uint64 Used[static_cast<int32>(EVoxelTerrainResourcePool::Count)] = {};
	FVoxelTerrainResourceLimits Limits;
};

class FVoxelTerrainResources;

/** A shared handle to one reservation. Copying the handle never grants bytes. */
class WHFRAMEWORK_API FVoxelTerrainResourceLease
{
public:
	FVoxelTerrainResourceLease(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InOwner, uint64 InSerial);
	~FVoxelTerrainResourceLease();
	FVoxelTerrainResourceLease(const FVoxelTerrainResourceLease&) = delete;
	FVoxelTerrainResourceLease& operator=(const FVoxelTerrainResourceLease&) = delete;

	bool Resize(uint64 InBytes);
	EVoxelTerrainResourceAdmission TryResize(uint64 InBytes, uint64 InRequiredInputBytes = 0, int32 InRequiredLeaseCount = 0);
	bool Transfer(EVoxelTerrainResourcePool InPool);
	// 消费原尝试预授予的目标池额度，保留正文租约身份并交给发布后的稳定所有者。
	bool CommitReservation(TSharedRef<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InReserved,
		const FVoxelGenerationConsumerKey& InNewOwner);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Split(uint64 InBytes);
	uint64 Bytes() const;
	EVoxelTerrainResourcePool GetPool() const;
	uint64 Serial() const;
	bool IsOwnedBy(const FVoxelTerrainResources& InOwner) const { return &Owner.Get() == &InOwner; }

private:
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Owner;
	uint64 LeaseSerial = 0;
};

/** Scheduler-owned byte ledger, shared by input, workers, Apply and publishers. */
class WHFRAMEWORK_API FVoxelTerrainResources final : public TSharedFromThis<FVoxelTerrainResources, ESPMode::ThreadSafe>
{
public:
	bool Configure(const FVoxelTerrainResourceLimits& InLimits);
	bool Acquire(const FVoxelGenerationConsumerKey& InAttempt, EVoxelTerrainResourcePool InPool,
		uint64 InBytes, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease,
		int32 InReservedSplits = 0);
	// 必需输入由原持有者声明；可释放的外部占用只报告暂时阻塞。
	EVoxelTerrainResourceAdmission TryAcquire(const FVoxelGenerationConsumerKey& InAttempt, EVoxelTerrainResourcePool InPool,
		uint64 InBytes, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease,
		int32 InReservedSplits = 0, uint64 InRequiredInputBytes = 0, int32 InRequiredLeaseCount = 0);
	FVoxelTerrainResourceSnapshot Snapshot() const;
	// owner 由完整 ConsumerKey 标识；同模块其它任务的释放仍可唤醒容量等待。
	// 最近两个不同 owner 的释放事件足以查询任一 owner 的最新外部释放，不增加监听表。
	uint64 ReleaseVersionExcludingOwner(const FVoxelGenerationConsumerKey& InOwner) const;
	bool TryGetReleaseVersionExcludingOwner(const FVoxelGenerationConsumerKey& InOwner, uint64& OutVersion) const;

private:
	friend class FVoxelTerrainResourceLease;
	struct FEntry
	{
		FVoxelGenerationConsumerKey Attempt;
		EVoxelTerrainResourcePool Pool = EVoxelTerrainResourcePool::Input;
		uint64 Bytes = 0;
		int32 ReservedSplits = 0;
	};
	bool CanAdd(EVoxelTerrainResourcePool InPool, uint64 InBytes) const;
	bool CanFitRequired(EVoxelTerrainResourcePool InPool, uint64 InBytes, int32 InReservedSplits,
		uint64 InRequiredInputBytes, int32 InRequiredLeaseCount) const;
	bool PrepareEntries(int32 InAdditionalEntries, uint64 InAdditionalInputBytes);
	bool Resize(uint64 InSerial, uint64 InBytes);
	EVoxelTerrainResourceAdmission TryResize(uint64 InSerial, uint64 InBytes, uint64 InRequiredInputBytes, int32 InRequiredLeaseCount);
	bool Transfer(uint64 InSerial, EVoxelTerrainResourcePool InPool);
	bool CommitReservation(uint64 InSerial, uint64 InReservedSerial,
		const FVoxelGenerationConsumerKey& InNewOwner);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Split(uint64 InSerial, uint64 InBytes);
	void Release(uint64 InSerial);
	uint64 Bytes(uint64 InSerial) const;
	EVoxelTerrainResourcePool GetPool(uint64 InSerial) const;
	void AdvanceVersion(bool bInCapacityReleased = false, const FVoxelGenerationConsumerKey* InReleasingOwner = nullptr);
	struct FReleaseEvent
	{
		FVoxelGenerationConsumerKey Consumer;
		uint64 Version = 0;
		bool IsOwner(const FVoxelGenerationConsumerKey& InOwner) const;
		bool IsOwner(const FReleaseEvent& InOther) const;
	};

	mutable FCriticalSection Mutex;
	FVoxelTerrainResourceSnapshot State;
	TMap<uint64, FEntry> Entries;
	int32 EntryCapacity = 0;
	uint64 NextSerial = 0;
	FReleaseEvent LatestRelease;
	FReleaseEvent PreviousRelease;
};
