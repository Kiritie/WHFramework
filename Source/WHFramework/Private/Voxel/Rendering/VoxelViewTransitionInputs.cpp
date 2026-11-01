#include "Voxel/Rendering/VoxelViewManager.h"

#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/Task/VoxelTerrainResources.h"

bool FVoxelViewManager::AreTransitionDirectoriesReady() const
{
	if (!InputDirectoryCapacityWaits[0].bWaiting && !InputDirectoryCapacityWaits[1].bWaiting) return true;
	const auto State = Scheduler.GetTerrainResources()->Snapshot();
	for (const auto& Wait : InputDirectoryCapacityWaits)
		if (Wait.bWaiting && State.ReleaseVersion == Wait.ReleaseVersion && State.Limits.InputBytes == Wait.InputLimit)
			return false;
	return true;
}

bool FVoxelViewManager::AcquireInputDirectoryCapacity(const uint8 InKind, const uint64 InBytes,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease)
{
	check(InKind < UE_ARRAY_COUNT(InputDirectoryCapacityWaits));
	const auto Resources = Scheduler.GetTerrainResources();
	const auto Before = Resources->Snapshot();
	auto& Wait = InputDirectoryCapacityWaits[InKind];
	if (Wait.bWaiting && Wait.ReleaseVersion == Before.ReleaseVersion && Wait.InputLimit == Before.Limits.InputBytes) return false;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, TaskOwnerId, TaskOwnerIncarnation,
		FIntVector(MIN_int32, 14, InKind), FMath::Max<uint64>(1, AppliedInterestRevision));
	const auto Admission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input, InBytes, OutLease);
	Wait.bWaiting = Admission != EVoxelTerrainResourceAdmission::Granted;
	Wait.ReleaseVersion = Before.ReleaseVersion;
	Wait.InputLimit = Before.Limits.InputBytes;
	if (Admission != EVoxelTerrainResourceAdmission::Granted && Admission != EVoxelTerrainResourceAdmission::CapacityBlocked)
		UE_LOG(LogTemp, Error, TEXT("Transition input directory admission failed: kind=%d bytes=%llu limit=%llu admission=%d"),
			InKind, InBytes, Before.Limits.InputBytes, int32(Admission));
	return !Wait.bWaiting;
}

TSharedPtr<FVoxelViewManager::FVolumeInputDirectory, ESPMode::ThreadSafe>
FVoxelViewManager::CreateVolumeInputDirectory(const int32 InCapacity)
{
	using namespace VoxelBoundedAllocation;
	const uint64 Bytes = WorkerSharedBytes<FVolumeInputDirectory>() + MapBytes<FVoxelViewKey, uint64>(InCapacity) +
		MapBytes<FVoxelViewKey, TSharedPtr<const FVoxelBoundaryTransitionContext>>(InCapacity);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	if (!AcquireInputDirectoryCapacity(0, Bytes, Lease)) return nullptr;
	auto Next = MakeWorkerShared<FVolumeInputDirectory>(Lease);
	Next->ResourceLease = MoveTemp(Lease);
	Next->Signatures.Reserve(InCapacity);
	Next->Contexts.Reserve(InCapacity);
	return Next;
}

TSharedPtr<FVoxelViewManager::FHeightfieldInputDirectory, ESPMode::ThreadSafe>
FVoxelViewManager::CreateHeightfieldInputDirectory(const int32 InCapacity, const uint64 InEdgeBytes)
{
	using namespace VoxelBoundedAllocation;
	const uint64 Bytes = WorkerSharedBytes<FHeightfieldInputDirectory>() + MapBytes<FVoxelHeightfieldNodeKey, uint64>(InCapacity) +
		MapBytes<FVoxelHeightfieldNodeKey, TSharedPtr<const FVoxelFrozenHeightfieldInputs, ESPMode::ThreadSafe>>(InCapacity) +
		MapBytes<FVoxelHeightfieldNodeKey, TArray<FVoxelHeightfieldTransitionEdge>>(InCapacity) + InEdgeBytes;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	if (!AcquireInputDirectoryCapacity(1, Bytes, Lease)) return nullptr;
	auto Next = MakeWorkerShared<FHeightfieldInputDirectory>(Lease);
	Next->ResourceLease = MoveTemp(Lease);
	Next->Signatures.Reserve(InCapacity);
	Next->Inputs.Reserve(InCapacity);
	Next->Edges.Reserve(InCapacity);
	return Next;
}

void FVoxelViewManager::ResetTransitionInputDirectories()
{
	// 同一租约已覆盖目录对象、容器和 worker 删除任务；GT 不遍历销毁大型冻结输入。
	if (VolumeInputs.ResourceLease)
		VoxelBoundedAllocation::MakeWorkerShared<FVolumeInputDirectory>(VolumeInputs.ResourceLease, MoveTemp(VolumeInputs));
	else VolumeInputs = {};
	if (HeightfieldInputs.ResourceLease)
		VoxelBoundedAllocation::MakeWorkerShared<FHeightfieldInputDirectory>(HeightfieldInputs.ResourceLease, MoveTemp(HeightfieldInputs));
	else HeightfieldInputs = {};
	for (auto& Wait : InputDirectoryCapacityWaits) Wait = {};
}
