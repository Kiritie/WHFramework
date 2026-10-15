#pragma once

#include "CoreMinimal.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTaskScheduler.h"

/** 同一生成尝试的合作式续跑与容量等待；自己的退款不唤醒自己。 */
class WHFRAMEWORK_API FVoxelGenerationCapacityWait
{
public:
	static TSharedPtr<FVoxelGenerationCapacityWait, ESPMode::ThreadSafe> Create(
		TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer)
	{
		if (!InResources || !InConsumer.IsValid()) return nullptr;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (!InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Input,
			VoxelBoundedAllocation::WorkerSharedBytes<FVoxelGenerationCapacityWait>(), Lease)) return nullptr;
		auto Wait = VoxelBoundedAllocation::MakeWorkerShared<FVoxelGenerationCapacityWait>(Lease);
		Wait->Lease = MoveTemp(Lease);
		Wait->Resources = MoveTemp(InResources);
		Wait->Consumer = InConsumer;
		return Wait;
	}

	uint64 Begin()
	{
		bWaiting.Store(false);
		return Resources->ReleaseVersionExcludingOwner(Consumer);
	}

	bool Continue(FVoxelTaskResult& InResult, const uint64 InBeforeBuild);

	EVoxelTaskBlockReason BlockReason() const
	{
		if (!bWaiting.Load()) return EVoxelTaskBlockReason::None;
		uint64 Current = 0;
		return Resources->TryGetReleaseVersionExcludingOwner(Consumer, Current) && Current > Version.Load()
			? EVoxelTaskBlockReason::None : EVoxelTaskBlockReason::Memory;
	}

private:
	// 最后释放租约；控制器的实际退休仍由 MakeWorkerShared 管理。
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TAtomic<bool> bWaiting{false};
	TAtomic<uint64> Version{0};
};
