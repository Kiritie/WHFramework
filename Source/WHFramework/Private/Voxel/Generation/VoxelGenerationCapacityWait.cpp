#include "Voxel/Generation/VoxelGenerationCapacityWait.h"

bool FVoxelGenerationCapacityWait::Continue(FVoxelTaskResult& InResult, const uint64 InBeforeBuild)
{
	if (InResult.bCanceled) return false;
	if (InResult.Error.StartsWith(TEXT("Yielded")))
	{
		bWaiting.Store(false);
		InResult.bSuccess = InResult.bContinue = true;
		// 只有生产者明确证明可在当前 Worker 继续，才使用调度器已有的有界时间片。
		InResult.bContinueImmediately = InResult.Error.StartsWith(TEXT("YieldedImmediate:"));
		InResult.Error.Reset();
		return true;
	}
	if (!InResult.Error.StartsWith(TEXT("CapacityBlocked"))) return false;
	Version.Store(InBeforeBuild);
	bWaiting.Store(true);
	InResult.bSuccess = InResult.bContinue = true;
	InResult.bContinueImmediately = false;
	InResult.Error.Reset();
	return true;
}
