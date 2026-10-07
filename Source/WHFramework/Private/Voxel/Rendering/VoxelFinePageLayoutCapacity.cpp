#include "Voxel/Rendering/VoxelFinePageLayout.h"

uint64 FVoxelFinePageLayout::GetAllocatedBytes() const
{
	uint64 Bytes = Anchors.GetAllocatedSize() + RootShards.GetAllocatedSize() + Pages.GetAllocatedSize() +
		RootPages.GetAllocatedSize() + DirtyQueue.GetAllocatedSize() + QueuedRoots.GetAllocatedSize();
	for (const auto& Pair : Anchors) Bytes += Pair.Value.Levels.GetAllocatedSize();
	for (const auto& Pair : RootShards) Bytes += Pair.Value.GetAllocatedSize();
	for (const auto& Pair : Pages) Bytes += Pair.Value.Shards.GetAllocatedSize();
	for (const auto& Pair : RootPages) Bytes += Pair.Value.GetAllocatedSize();
	return Bytes;
}

TSharedPtr<FVoxelFinePageLayout, ESPMode::ThreadSafe> FVoxelFinePageLayout::Capture(
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer, FString& OutError) const
{
	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelFinePageLayout>() + GetAllocatedBytes();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	if (InResources)
	{
		if (Bytes > InResources->Snapshot().Limits.InputBytes)
		{ OutError = TEXT("Oversized: Fine Page layout root"); return nullptr; }
		if (!InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Input, Bytes, Lease))
		{ OutError = TEXT("CapacityBlocked: Fine Page layout root"); return nullptr; }
	}
	auto Result = Lease ? VoxelBoundedAllocation::MakeWorkerShared<FVoxelFinePageLayout>(Lease, *this) :
		MakeShared<FVoxelFinePageLayout, ESPMode::ThreadSafe>(*this);
	Result->Resources = MoveTemp(InResources); Result->Consumer = InConsumer; Result->StorageLease = MoveTemp(Lease);
	check(Result->GetAllocatedBytes() <= GetAllocatedBytes());
	OutError.Reset();
	return Result;
}

bool FVoxelFinePageLayout::GrantStorage(const uint64 InBytes, FString& OutError)
{
	if (!Resources) return true;
	const uint64 Peak = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelFinePageLayout>() + GetAllocatedBytes() + InBytes;
	if (Peak > Resources->Snapshot().Limits.InputBytes)
	{ OutError = TEXT("Oversized: Fine Page layout allocation peak"); return false; }
	if ((!StorageLease && !Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, Peak, StorageLease)) ||
		!StorageLease->Resize(Peak))
	{ OutError = TEXT("CapacityBlocked: Fine Page layout allocation peak"); return false; }
	return true;
}

void FVoxelFinePageLayout::SettleStorage()
{
	if (StorageLease)
	{
		const uint64 ActualBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelFinePageLayout>() + GetAllocatedBytes();
		check(ActualBytes <= StorageLease->Bytes());
		const bool bSuccess = StorageLease->Resize(ActualBytes);
		check(bSuccess);
	}
}

bool FVoxelFinePageLayout::ReserveSource(const int32 InSourceCount, FString& OutError)
{
	const int64 QueueRows64 = int64(DirtyQueue.Num()) + RootShards.Num();
	if (InSourceCount < 0 || QueueRows64 > MAX_int32)
	{ OutError = TEXT("Oversized: Fine Page Source rows"); return false; }
	const int32 QueueRows = int32(QueueRows64);
	const uint64 NewBytes = GetAllocatedBytes() +
		VoxelBoundedAllocation::MapBytes<FGuid, FVoxelFineLevelAnchors>(InSourceCount) +
		VoxelBoundedAllocation::ArrayBytes<FIntVector>(MaximumLevel + 1) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelFinePageKey>(QueueRows) +
		VoxelBoundedAllocation::SetBytes<FVoxelFinePageKey>(QueueRows);
	if (!GrantStorage(NewBytes, OutError)) return false;
	Anchors.Reserve(InSourceCount); DirtyQueue.Reserve(QueueRows); QueuedRoots.Reserve(QueueRows);
	return true;
}

bool FVoxelFinePageLayout::ReserveResidencyDelta(const TVoxelFineSetDelta<FVoxelFineShardKey>& InDelta, FString& OutError)
{
	const int64 ChangeRows64 = int64(InDelta.Added.Num()) + InDelta.Removed.Num();
	if (ChangeRows64 > MAX_int32)
	{ OutError = TEXT("Oversized: Fine Page dirty root rows"); return false; }
	const int32 ChangeRows = int32(ChangeRows64);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
	if (Resources && !Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working,
		FMath::Max<uint64>(1, VoxelBoundedAllocation::MapBytes<FVoxelFinePageKey, int32>(ChangeRows)), Scratch))
	{ OutError = TEXT("CapacityBlocked: Fine Page delta scratch"); return false; }
	TMap<FVoxelFinePageKey, int32> Changes;
	Changes.Reserve(ChangeRows);
	for (const auto& Key : InDelta.Added) ++Changes.FindOrAdd(RootFor(Key));
	for (const auto& Key : InDelta.Removed) Changes.FindOrAdd(RootFor(Key));
	const int64 RootRows64 = int64(RootShards.Num()) + Changes.Num();
	const int64 QueueRows64 = int64(DirtyQueue.Num()) + Changes.Num();
	if (RootRows64 > MAX_int32 || QueueRows64 > MAX_int32)
	{ OutError = TEXT("Oversized: Fine Page root rows"); return false; }
	uint64 NewBytes = GetAllocatedBytes() +
		VoxelBoundedAllocation::MapBytes<FVoxelFinePageKey, TSet<FVoxelFineShardKey>>(int32(RootRows64)) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelFinePageKey>(int32(QueueRows64)) +
		VoxelBoundedAllocation::SetBytes<FVoxelFinePageKey>(int32(QueueRows64));
	for (const auto& Pair : Changes)
	{
		const auto* Previous = RootShards.Find(Pair.Key);
		const int64 Count64 = int64(Previous ? Previous->Num() : 0) + Pair.Value;
		if (Count64 > MAX_int32) { OutError = TEXT("Oversized: Fine Page root Shards"); return false; }
		NewBytes += VoxelBoundedAllocation::SetBytes<FVoxelFineShardKey>(int32(Count64));
	}
	if (!GrantStorage(NewBytes, OutError)) return false;
	RootShards.Reserve(int32(RootRows64)); DirtyQueue.Reserve(int32(QueueRows64)); QueuedRoots.Reserve(int32(QueueRows64));
	for (const auto& Pair : Changes)
	{
		if (Pair.Value == 0) continue;
		auto& Shards = RootShards.FindOrAdd(Pair.Key);
		Shards.Reserve(Shards.Num() + Pair.Value);
	}
	return true;
}

bool FVoxelFinePageLayout::ReserveAdvance(const FVoxelFinePageKey& InRoot, const int32 InShardCount,
	FVoxelFinePageLayoutDelta& OutDelta, const bool bInProduceDelta,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutScratch, FString& OutError)
{
	const auto* Previous = RootPages.Find(InRoot);
	const int32 PreviousPages = Previous ? Previous->Num() : 0;
	const int64 PageRows64 = int64(Pages.Num()) + InShardCount;
	const int64 AddedRows64 = int64(OutDelta.Added.Num()) + InShardCount;
	const int64 RemovedRows64 = int64(OutDelta.Removed.Num()) + PreviousPages;
	const int64 RetainedRows64 = int64(OutDelta.Retained.Num()) + InShardCount;
	if (PageRows64 > MAX_int32 || AddedRows64 > MAX_int32 || RemovedRows64 > MAX_int32 ||
		RetainedRows64 > MAX_int32 || RootPages.Num() == MAX_int32)
	{ OutError = TEXT("Oversized: Fine Page advance rows"); return false; }
	const uint64 ShardArrayBytes = uint64(InShardCount) * VoxelBoundedAllocation::ArrayBytes<FVoxelFineShardKey>(1);
	const uint64 ScratchBytes = VoxelBoundedAllocation::MapBytes<FVoxelFinePageKey, FVoxelFineLayoutPage>(InShardCount) +
		2 * VoxelBoundedAllocation::SetBytes<FVoxelFinePageKey>(InShardCount) +
		2 * ShardArrayBytes;
	if (Resources)
	{
		if (ScratchBytes > Resources->Snapshot().Limits.WorkingBytes)
		{ OutError = TEXT("Oversized: Fine Page advance scratch"); return false; }
		if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, FMath::Max<uint64>(1, ScratchBytes), OutScratch))
		{ OutError = TEXT("CapacityBlocked: Fine Page advance scratch"); return false; }
	}
	if (bInProduceDelta)
	{
		uint64 NestedBytes = ShardArrayBytes;
		for (const auto& Page : OutDelta.Added) NestedBytes += Page.Shards.GetAllocatedSize();
		const uint64 NewDeltaBytes = VoxelBoundedAllocation::ArrayBytes<FVoxelFineLayoutPage>(int32(AddedRows64)) +
			VoxelBoundedAllocation::SetBytes<FVoxelFinePageKey>(int32(RemovedRows64)) +
			VoxelBoundedAllocation::SetBytes<FVoxelFinePageKey>(int32(RetainedRows64)) + NestedBytes;
		const uint64 OldDeltaBytes = OutDelta.Added.GetAllocatedSize() + OutDelta.Removed.GetAllocatedSize() + OutDelta.Retained.GetAllocatedSize();
		if (Resources)
		{
			if (OldDeltaBytes + NewDeltaBytes > Resources->Snapshot().Limits.InputBytes)
			{ OutError = TEXT("Oversized: Fine Page output delta"); return false; }
			if ((!OutDelta.ResourceLease && !Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
				FMath::Max<uint64>(1, OldDeltaBytes + NewDeltaBytes), OutDelta.ResourceLease)) ||
				!OutDelta.ResourceLease->Resize(FMath::Max<uint64>(1, OldDeltaBytes + NewDeltaBytes)))
			{ OutError = TEXT("CapacityBlocked: Fine Page output delta"); return false; }
		}
		OutDelta.Added.Reserve(int32(AddedRows64)); OutDelta.Removed.Reserve(int32(RemovedRows64));
		OutDelta.Retained.Reserve(int32(RetainedRows64));
	}
	const uint64 NewBytes = GetAllocatedBytes() +
		VoxelBoundedAllocation::MapBytes<FVoxelFinePageKey, FVoxelFineLayoutPage>(int32(PageRows64)) +
		VoxelBoundedAllocation::MapBytes<FVoxelFinePageKey, TSet<FVoxelFinePageKey>>(RootPages.Num() + 1) +
		VoxelBoundedAllocation::SetBytes<FVoxelFinePageKey>(InShardCount) + ShardArrayBytes;
	if (!GrantStorage(NewBytes, OutError)) return false;
	Pages.Reserve(int32(PageRows64)); RootPages.Reserve(RootPages.Num() + 1);
	return true;
}
