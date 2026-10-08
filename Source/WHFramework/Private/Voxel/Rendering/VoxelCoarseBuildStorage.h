#pragma once

#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTaskScheduler.h"

// 输入控制结构、微步 scratch 和最终 CPU 输出共用 Scheduler 四账本。
// 第一次执行先保留完整输出容量，输入不能把完成路径挤出 Staged。
struct FVoxelCoarseBuildStorage
{
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> GeometryLease;
	uint64 GeometryCapacityBytes = 0;
	uint64 WorkingCapacityBytes = 8ull * 1024ull * 1024ull;
	TAtomic<bool> bWaitingCapacity{false};
	TAtomic<uint64> CapacityVersion{0};

	FVoxelCoarseBuildStorage(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InMetadata)
		: Resources(InResources), Consumer(InConsumer), MetadataLease(MoveTemp(InMetadata))
	{
	}

	EVoxelTaskBlockReason BlockReason() const
	{
		if (!bWaitingCapacity.Load()) return EVoxelTaskBlockReason::None;
		uint64 Version = 0;
		return !Resources->TryGetReleaseVersionExcludingOwner(Consumer, Version) || Version <= CapacityVersion.Load()
			? EVoxelTaskBlockReason::Memory : EVoxelTaskBlockReason::None;
	}

	void DeferCapacity(FVoxelTaskResult& OutResult, const uint64 InBeforeAdmission)
	{
		CapacityVersion.Store(InBeforeAdmission);
		bWaitingCapacity.Store(true);
		OutResult.Error.Reset();
		OutResult.bSuccess = OutResult.bContinue = true;
	}

	bool Begin(FVoxelTaskResult& OutResult,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutWorking)
	{
		if (GeometryCapacityBytes == 0 || GeometryCapacityBytes > Resources->Snapshot().Limits.StagedBytes)
		{
			OutResult.Error = TEXT("Oversized: coarse completion cannot fit Staged capacity");
			return false;
		}
		const uint64 BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
		if ((!GeometryLease && !Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged,
			GeometryCapacityBytes, GeometryLease)) ||
			(WorkingCapacityBytes && !Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, WorkingCapacityBytes, OutWorking)))
		{
			DeferCapacity(OutResult, BeforeAdmission);
			return false;
		}
		bWaitingCapacity.Store(false);
		return true;
	}

	bool BeginCount(FVoxelTaskResult& OutResult,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutWorking)
	{
		const uint64 BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
		if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, WorkingCapacityBytes, OutWorking))
		{
			DeferCapacity(OutResult, BeforeAdmission);
			return false;
		}
		bWaitingCapacity.Store(false);
		return true;
	}

	template<typename ObjectType>
	TSharedPtr<ObjectType, ESPMode::ThreadSafe> MakeResult() const
	{
		auto Result = VoxelBoundedAllocation::MakeWorkerShared<ObjectType>(GeometryLease);
		Result->ResourceLease = GeometryLease;
		return Result;
	}
};
