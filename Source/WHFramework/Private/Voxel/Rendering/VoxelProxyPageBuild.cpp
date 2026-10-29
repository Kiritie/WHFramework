#include "Voxel/Rendering/VoxelProxyPageBuild.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "HAL/ThreadSafeCounter64.h"

namespace
{
	void RecordProxyColumnReuse(const FVoxelProxyColumnWorkspace& InColumns, const int32 InSide)
	{
#if !UE_BUILD_SHIPPING
		static FThreadSafeCounter64 Completed;
		static FThreadSafeCounter64 Sampled;
		static FThreadSafeCounter64 Reused;
		const uint64 Count = uint64(Completed.Increment());
		const uint64 SampledTotal = uint64(Sampled.Add(int64(InColumns.SampledColumns))) + InColumns.SampledColumns;
		const uint64 ReusedTotal = uint64(Reused.Add(int64(InColumns.ReusedColumns))) + InColumns.ReusedColumns;
		if (Count <= 8 || Count % 128 == 0)
			UE_LOG(LogTemp, Display, TEXT("Voxel proxy shared columns: completed=%llu side=%d sampled=%llu reused=%llu sampledTotal=%llu reusedTotal=%llu workingBytes=%llu"),
				Count, InSide, InColumns.SampledColumns, InColumns.ReusedColumns, SampledTotal, ReusedTotal,
				InColumns.Lease ? InColumns.Lease->Bytes() : 0);
#endif
	}

	void RecordProxySurfaceStorage(const FVoxelVoxelProxyData& InData)
	{
#if !UE_BUILD_SHIPPING
		static FThreadSafeCounter64 Completed;
		static FThreadSafeCounter64 DenseBytes;
		static FThreadSafeCounter64 StoredBytes;
		const uint64 Count = uint64(Completed.Increment());
		const uint64 Dense = uint64(InData.Cells.Num()) * sizeof(FVoxelBlockState);
		const uint64 Stored = InData.Cells.GetAllocatedSize();
		const uint64 DenseTotal = uint64(DenseBytes.Add(int64(Dense))) + Dense;
		const uint64 StoredTotal = uint64(StoredBytes.Add(int64(Stored))) + Stored;
		if (Count <= 8 || Count % 128 == 0)
			UE_LOG(LogTemp, Display, TEXT("Voxel proxy compact storage: completed=%llu side=%d logical=%d columns=%d overrides=%d denseBytes=%llu storedBytes=%llu denseTotal=%llu storedTotal=%llu"),
				Count, InData.GridSide, InData.Cells.Num(), InData.Cells.GetColumns().Num(), InData.Cells.GetOverrides().Num(), Dense, Stored, DenseTotal, StoredTotal);
#endif
	}
}

bool FVoxelProxyPageBuild::Initialize(const FVoxelTerrainGeometry& InGeometry, FString& OutError)
{
	if (!InGeometry.Validate(OutError))
	{
		return false;
	}
	if (InGeometry.Key.Representation != EVoxelTerrainRepresentation::VoxelProxy || InGeometry.Key.Level == 0)
	{
		OutError = TEXT("Proxy page requires an explicit coarse Proxy geometry");
		return false;
	}
	FVoxelProxyPageBuild Candidate;
	Candidate.Geometry = InGeometry;
	if (Candidate.MaximumRetainedBytes() > 32ull * 1024ull * 1024ull)
	{
		OutError = TEXT("Oversized: proxy page completion exceeds retained-input capacity");
		return false;
	}
	if (!VoxelStreamingGeometry::BuildShards(InGeometry, 16, 4096, Candidate.Shards, OutError))
	{
		return false;
	}
	Columns.Reset();
	*this = MoveTemp(Candidate);
	return true;
}

uint64 FVoxelProxyPageBuild::MaximumRetainedBytes() const
{
	using namespace VoxelBoundedAllocation;
	const int64 Side = Geometry.Key.SideCells() / FMath::Max(1, Geometry.SampleStepCells);
	if (Side <= 0 || Side > 256) return MAX_uint64;
	const int32 Parts = int32((Side + 15) / 16);
	return WorkerSharedBytes<FVoxelProxyPageBuild>() + ArrayBytes<FVoxelTerrainBuildShard>(Parts * Parts * Parts) +
		ArrayBytes<FVoxelProxyColumn>(int32(Side * Side)) + ArrayBytes<FVoxelProxyCell>(int32(Side * Side * Side)) +
		ArrayBytes<FVoxelProxyCell>(int32(Side * Side * Side / 2)) +
		6 * ArrayBytes<FVoxelBlockState>(int32(Side * Side)) +
		ArrayBytes<FVoxelCoverageRect>(int32(Side * ((Side + 1) / 2))) +
		ArrayBytes<FVoxelProxyColumn>(16 * 16) + ArrayBytes<FVoxelProxyCell>(16 * 16 * 16) +
		ArrayBytes<FVoxelProxyCell>(16 * 16 * 8) + 6 * ArrayBytes<FVoxelBlockState>(16 * 16);
}

bool FVoxelProxyPageBuild::Advance(const FVoxelVoxelProxyBuilder& InBuilder,
	const FVoxelOverlaySnapshotSet& InOverlays, FVoxelVoxelProxyData& OutData,
	bool& bOutComplete, FString& OutError, const TAtomic<bool>* InCancel)
{
	bOutComplete = false;
	if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return false; }
	if (NextShard == Shards.Num())
	{
		Page.Cells.Sort();
		if (!Page.AdvanceSurfaceCoverage(1, bOutComplete, OutError, InCancel)) return false;
		if (bOutComplete) { RecordProxySurfaceStorage(Page); OutData = MoveTemp(Page); ++NextShard; }
		return true;
	}
	if (!Shards.IsValidIndex(NextShard)) { OutError = TEXT("Proxy page result was already taken"); return false; }
	const FVoxelViewKey Key{Geometry.Key.Coordinate, Geometry.Key.Level};
	const int32 Side = int32(Geometry.Key.SideCells() / Geometry.SampleStepCells);
	if (!bAllocated)
	{
		Page.Key = Key;
		Page.Geometry = Geometry;
		Page.GridSide = Side;
		for (int32 Face = 0; Face < 6; ++Face)
		{ Page.Halo[Face].Reserve(Side * Side); Page.Halo[Face].SetNumUninitialized(Side * Side); Page.Known[Face] = true; }
		bAllocated = true;
	}
	FVoxelVoxelProxyData Part;
	if (!InBuilder.Build(Key, InOverlays, Part, OutError, InCancel, Geometry, &Shards[NextShard], &Columns)) return false;
	if (Page.Cells.IsEmpty()) Page.Cells.InitializeColumns(Side, Part.Cells.GetInterior());
	const FIntVector Offset = Part.LocalMinCells / Geometry.SampleStepCells;
	const int32 Count = Part.GridSide;
	if (Part.Cells.Num() != Count * Count * Count) { OutError = TEXT("Invalid proxy execution window"); return false; }
	Page.Cells.AppendShard(Part.Cells, Offset);
	for (int32 Face = 0; Face < 6; ++Face)
	{
		const int32 Axis = Face / 2, U = (Axis + 1) % 3, V = (Axis + 2) % 3;
		if ((Face & 1) ? Offset[Axis] != 0 : Offset[Axis] + Count != Side) continue;
		if (Part.Halo[Face].Num() != Count * Count) { OutError = TEXT("Invalid proxy halo window"); return false; }
		Page.Known[Face] &= Part.Known[Face];
		for (int32 Y = 0; Y < Count; ++Y)
			for (int32 X = 0; X < Count; ++X)
				Page.Halo[Face][Offset[U] + X + Side * (Offset[V] + Y)] = Part.Halo[Face][X + Count * Y];
	}
	Page.Revision = FMath::Max(Page.Revision, Part.Revision);
	Page.bHasVisibleSurfaceEvidence |= Part.bHasVisibleSurfaceEvidence;
	// 每次只复制一个有界窗口；最终一步仅转移数组所有权。
	if (++NextShard == Shards.Num())
	{
		RecordProxyColumnReuse(Columns, Side);
		Columns.Reset();
		Page.Cells.Sort();
		if (!Page.AdvanceSurfaceCoverage(1, bOutComplete, OutError, InCancel)) return false;
		if (bOutComplete) { RecordProxySurfaceStorage(Page); OutData = MoveTemp(Page); ++NextShard; }
	}
	return true;
}
