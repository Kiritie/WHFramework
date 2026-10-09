#include "Voxel/Rendering/VoxelViewManager.h"

#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/VoxelModule.h"

void FVoxelViewManager::BindCoverageIndexResources()
{
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, TaskOwnerId, TaskOwnerIncarnation, {}, 1);
	const auto Resources = Scheduler.GetTerrainResources();
	CoverageIndex.BindResources(Resources, Consumer);
	HeightfieldCoverageIndex.BindResources(Resources, Consumer);
	EmptyInputRoot.TerrainRootIndex.BindResources(Resources, Consumer);
}

bool FVoxelViewManager::ReserveCoverageIndexRetries(const int32 InCount)
{
	if (InCount <= CoverageIndexRetryCapacity) return true;
	const int64 Next = FMath::Max<int64>(InCount, FMath::Max<int64>(16, int64(CoverageIndexRetryCapacity) * 2));
	if (Next > MAX_int32) return false;
	const int32 Capacity = int32(Next);
	const uint64 Bytes = VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FVoxelTerrainSpatialKey, uint64>>(CoverageIndexRetries, Capacity);
	const uint64 Previous = VoxelGenerationAllocation::AllocatedBytes(CoverageIndexRetries);
	if (CoverageIndexRetryLease)
	{
		if (!CoverageIndexRetryLease->Resize(Previous + Bytes)) return false;
	}
	else if (!Scheduler.GetTerrainResources()->Acquire(
		FVoxelGenerationConsumerKey::Section(WorldEpoch, TaskOwnerId, TaskOwnerIncarnation, {}, 1),
		EVoxelTerrainResourcePool::Input, Previous + Bytes, CoverageIndexRetryLease)) return false;
	CoverageIndexRetries.Reserve(Capacity);
	const uint64 Actual = VoxelGenerationAllocation::AllocatedBytes(CoverageIndexRetries);
	check(Actual <= Bytes);
	verify(CoverageIndexRetryLease->Resize(Actual));
	CoverageIndexRetryCapacity = Capacity;
	return true;
}

bool FVoxelViewManager::ApplyCoverageIndexRecord(const FVoxelCoverageRecord& InRecord)
{
	const auto& Key = InRecord.Geometry.Key;
	// 生产工作区提前按全部潜在键预留。独立调用也必须先保证失败记录槽，再尝试 COW 路径。
	if (!CoverageIndexRetries.Contains(Key) && !ReserveCoverageIndexRetries(CoverageIndexRetries.Num() + 1)) return false;
	FString Error;
	const bool bRemove = !InRecord.bWanted && !InRecord.bReady && !InRecord.bCommitted;
	const auto Apply = [&InRecord, bRemove, &Error](FVoxelCoverageIndex& Index)
	{
		if (!bRemove) return Index.Upsert(InRecord, Error);
		Index.Remove(InRecord.Geometry.Key, Error);
		return Error.IsEmpty();
	};
	const bool bComplete = Apply(CoverageIndex) && (!Key.IsColumn() || Apply(HeightfieldCoverageIndex));
	if (bComplete)
	{
		CoverageIndexRetries.Remove(Key);
		return true;
	}
	if (Error.StartsWith(TEXT("CapacityBlocked")))
	{
		// 只保留命中键和释放代；恢复时重新读取当前事实，不能重放过期 Record 或扫整张世界表。
		check(CoverageIndexRetries.Num() < CoverageIndexRetryCapacity || CoverageIndexRetries.Contains(Key));
		CoverageIndexRetries.FindOrAdd(Key) = Scheduler.GetTerrainResources()->Snapshot().ReleaseVersion;
		return false;
	}
	ensureMsgf(false, TEXT("Invalid immutable coverage input: %s"), *Error);
	return false;
}

void FVoxelViewManager::AdvanceCoverageIndexRecovery(const double InDeadline)
{
	if (CoverageIndexRetries.IsEmpty()) { CoverageIndexRetryCursor = 0; return; }
	const uint64 ReleaseVersion = Scheduler.GetTerrainResources()->Snapshot().ReleaseVersion;
	const int32 Maximum = CoverageIndexRetries.GetMaxIndex();
	for (int32 Step = 0; Step < FMath::Min(8192, Maximum) && FPlatformTime::Seconds() < InDeadline; ++Step)
	{
		if (CoverageIndexRetryCursor >= Maximum) CoverageIndexRetryCursor = 0;
		const auto Id = FSetElementId::FromInteger(CoverageIndexRetryCursor++);
		if (!CoverageIndexRetries.IsValidId(Id)) continue;
		const auto& Entry = CoverageIndexRetries.Get(Id);
		if (ReleaseVersion <= Entry.Value) continue;
		const double Start = FPlatformTime::Seconds();
		if (!TerrainFrameBudget.TryStart(EVoxelPublishPhase::Plan, Start, InterestMicrostepEstimateMilliseconds)) break;
		const auto Key = Entry.Key;
		if (Key.Representation == EVoxelTerrainRepresentation::Fine || Key.Representation == EVoxelTerrainRepresentation::VoxelProxy)
			IndexCoverage(FVoxelViewKey{Key.Coordinate, Key.Level});
		else if (Key.Representation == EVoxelTerrainRepresentation::Surface)
			IndexCoverage(FVoxelSurfaceTileKey{FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
		else IndexCoverage(FVoxelMacroTileKey{FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
		const double End = FPlatformTime::Seconds();
		InterestMicrostepEstimateMilliseconds = FMath::Lerp(InterestMicrostepEstimateMilliseconds,
			FMath::Max(0.001, (End - Start) * 1000.0), 0.15);
		TerrainFrameBudget.Finish(EVoxelPublishPhase::Plan, Start, End);
	}
}
