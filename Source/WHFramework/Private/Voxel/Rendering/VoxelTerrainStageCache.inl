// 私有缓存节点保存完整空间输入；共享子树只保留引用，变化路径在 Worker 重建。
struct FVoxelTerrainStageCacheNode
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	TArray<FVoxelTerrainStageSource> Sources;
	TArray<FVoxelTerrainGeometry> Predecessors;
	TSharedPtr<const FVoxelTerrainStageCacheNode, ESPMode::ThreadSafe> Children[8];
	FVoxelTerrainGeometry Geometry;
	FVoxelTerrainPageCost Cost;
	int32 Priority = MAX_int32;
	bool bPage = false;
	bool bFloor = false;
};

struct FVoxelTerrainStageCacheIndex
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	TMap<FVoxelTerrainSpatialKey, TSharedPtr<const FVoxelTerrainStageCacheNode, ESPMode::ThreadSafe>> Roots;
	TSet<FVoxelTerrainSpatialKey> FailedRoots;
	uint8 MaximumLevel = 0;
	int32 MaximumSamples = 0, MinZ = 0, MaxZ = 0;
	double TargetPixels = 0.0;
	uint64 Generator = 0, Material = 0;
	FVoxelTerrainPageCostLimits Limits;

	bool SamePolicy(uint8 Level, int32 Samples, int32 WorldMin, int32 WorldMax, double Pixels,
		uint64 GeneratorIdentity, uint64 MaterialRevision, const FVoxelTerrainPageCostLimits& CostLimits) const
	{
		return MaximumLevel == Level && MaximumSamples == Samples && MinZ == WorldMin && MaxZ == WorldMax &&
			TargetPixels == Pixels && Generator == GeneratorIdentity && Material == MaterialRevision &&
			Limits.Triangles == CostLimits.Triangles && Limits.MeshBytes == CostLimits.MeshBytes &&
			Limits.BuildMilliseconds == CostLimits.BuildMilliseconds;
	}
};

namespace
{
	using FStageCacheNodePtr = TSharedPtr<const FVoxelTerrainStageCacheNode, ESPMode::ThreadSafe>;

	uint64 StagePlanBytes(const int32 Count)
	{
		return VoxelBoundedAllocation::MapBytes<FVoxelTerrainSpatialKey, FVoxelTerrainGeometry>(Count) +
			VoxelBoundedAllocation::MapBytes<FVoxelTerrainSpatialKey, FVoxelTerrainPageCost>(Count) +
			VoxelBoundedAllocation::MapBytes<FVoxelTerrainSpatialKey, int32>(Count) +
			VoxelBoundedAllocation::SetBytes<FVoxelTerrainSpatialKey>(Count);
	}

	bool ReserveStagePlan(FVoxelTerrainStagePlan& Plan, const int32 Required, FString& Error)
	{
		if (Required < 0 || Required > (1 << 20)) { Error = TEXT("CapacityUnachievable: coarse plan metadata"); return false; }
		if (Required <= Plan.ReservedPages) return true;
		const int32 Capacity = FMath::Max(Required, FMath::Min(1 << 20, FMath::Max(8, Plan.ReservedPages * 2)));
		const uint64 Bytes = StagePlanBytes(Capacity);
		if (Plan.Resources)
		{
			const uint64 Peak = Bytes + (Plan.ResourceLease ? Plan.ResourceLease->Bytes() : 0);
			if (Peak > Plan.Resources->Snapshot().Limits.WorkingBytes)
			{ Error = TEXT("CapacityUnachievable: coarse plan growth peak"); return false; }
			const bool bGranted = Plan.ResourceLease ? Plan.ResourceLease->Resize(Peak) :
				Plan.Resources->Acquire(Plan.Consumer, EVoxelTerrainResourcePool::Working, Peak, Plan.ResourceLease);
			if (!bGranted) { Error = TEXT("CapacityBlocked: coarse plan containers"); return false; }
		}
		Plan.Pages.Reserve(Capacity); Plan.Costs.Reserve(Capacity);
		Plan.Priorities.Reserve(Capacity); Plan.ResolutionFloorPages.Reserve(Capacity);
		Plan.ReservedPages = Capacity;
		if (Plan.ResourceLease) verify(Plan.ResourceLease->Resize(Bytes));
		return true;
	}

	bool AppendStagePage(FVoxelTerrainStagePlan& Plan, const FVoxelTerrainGeometry& Geometry,
		const FVoxelTerrainPageCost& Cost, const int32 Priority, const bool bFloor, FString& Error)
	{
		if (!ReserveStagePlan(Plan, Plan.Pages.Num() + (Plan.Pages.Contains(Geometry.Key) ? 0 : 1), Error)) return false;
		Plan.Pages.Add(Geometry.Key, Geometry); Plan.Costs.Add(Geometry.Key, Cost);
		Plan.Priorities.Add(Geometry.Key, Priority);
		if (bFloor) Plan.ResolutionFloorPages.Add(Geometry.Key);
		else Plan.ResolutionFloorPages.Remove(Geometry.Key);
		return true;
	}

	template<typename ElementType>
	bool ReserveStageScratch(TArray<ElementType>& Array, const int32 Required, const uint64 OtherBytes,
		const int32 Maximum, FVoxelTerrainStagePlan& Plan,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease, FString& Error)
	{
		if (Required < 0 || Required > Maximum) { Error = TEXT("CapacityUnachievable: coarse scratch count"); return false; }
		if (Required <= Array.Max()) return true;
		const int32 Capacity = FMath::Max(Required, FMath::Min(Maximum, FMath::Max(8, Array.Max() * 2)));
		const uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<ElementType>(Capacity);
		if (Plan.Resources)
		{
			const uint64 Peak = OtherBytes + Array.GetAllocatedSize() + Bytes;
			if (Peak > Plan.Resources->Snapshot().Limits.WorkingBytes)
			{ Error = TEXT("CapacityUnachievable: coarse scratch peak"); return false; }
			const bool bGranted = Lease ? Lease->Resize(Peak) :
				Plan.Resources->Acquire(Plan.Consumer, EVoxelTerrainResourcePool::Working, Peak, Lease);
			if (!bGranted) { Error = TEXT("CapacityBlocked: coarse scratch"); return false; }
		}
		Array.Reserve(Capacity);
		if (Lease) verify(Lease->Resize(OtherBytes + Array.GetAllocatedSize()));
		return true;
	}

	bool SameStageInfluence(const FVoxelTerrainStageSource& A, const FVoxelTerrainStageSource& B)
	{
		return A.Source.Id == B.Source.Id && A.Source.Center == B.Source.Center &&
			A.Source.VerticalFovDegrees == B.Source.VerticalFovDegrees && A.Source.ViewportHeightPixels == B.Source.ViewportHeightPixels &&
			A.OuterRadiusCells == B.OuterRadiusCells && A.InnerRadiusCells == B.InnerRadiusCells &&
			A.VerticalHalfExtentCells == B.VerticalHalfExtentCells;
	}

	bool StageInfluences(const FVoxelTerrainSpatialKey& Key, const FIntVector& Min, const FIntVector& Max,
		const FVoxelTerrainStageSource& Entry)
	{
		const auto& Center = Entry.Source.Center;
		const double X = double(Center.X) - FMath::Clamp(double(Center.X), double(Min.X), double(Max.X));
		const double Y = double(Center.Y) - FMath::Clamp(double(Center.Y), double(Min.Y), double(Max.Y));
		return Entry.OuterRadiusCells > 0 && X * X + Y * Y <= FMath::Square(double(Entry.OuterRadiusCells)) &&
			(Key.IsColumn() || (int64(Max.Z) >= int64(Center.Z) - Entry.VerticalHalfExtentCells &&
				int64(Min.Z) <= int64(Center.Z) + Entry.VerticalHalfExtentCells));
	}

	bool StageCacheGuidBefore(const FGuid& A, const FGuid& B)
	{
		if (A.A != B.A) return A.A < B.A;
		if (A.B != B.B) return A.B < B.B;
		if (A.C != B.C) return A.C < B.C;
		return A.D < B.D;
	}

	bool StageCacheGeometryBefore(const FVoxelTerrainGeometry& A, const FVoxelTerrainGeometry& B)
	{
		if (A.Key.Representation != B.Key.Representation) return uint8(A.Key.Representation) < uint8(B.Key.Representation);
		if (A.Key.Coordinate.X != B.Key.Coordinate.X) return A.Key.Coordinate.X < B.Key.Coordinate.X;
		if (A.Key.Coordinate.Y != B.Key.Coordinate.Y) return A.Key.Coordinate.Y < B.Key.Coordinate.Y;
		if (A.Key.Coordinate.Z != B.Key.Coordinate.Z) return A.Key.Coordinate.Z < B.Key.Coordinate.Z;
		return A.Key.Level < B.Key.Level;
	}

	bool PrepareStageCacheNode(const FVoxelTerrainSpatialKey& Key, const FIntVector& Min, const FIntVector& Max,
		TConstArrayView<FVoxelTerrainStageSource> Sources, const FVoxelCoverageIndex* Predecessor,
		FVoxelTerrainStageCache& Cache, TSharedPtr<FVoxelTerrainStageCacheNode, ESPMode::ThreadSafe>& Out, FString& Error)
	{
		int32 SourceCount = 0;
		for (const auto& Source : Sources) SourceCount += StageInfluences(Key, Min, Max, Source);
		FVoxelCoverageLocalSnapshot Counts;
		FIntVector QueryMin, QueryMax;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			QueryMin[Axis] = int32(FMath::Max<int64>(MIN_int32, int64(Min[Axis]) - 1));
			QueryMax[Axis] = int32(FMath::Min<int64>(MAX_int32, int64(Max[Axis]) + 1));
		}
		if (Predecessor) Predecessor->CountCapture(QueryMin, QueryMax, Counts);
		const uint64 InputBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainStageCacheNode>() +
			VoxelBoundedAllocation::ArrayBytes<FVoxelTerrainStageSource>(SourceCount) +
			VoxelBoundedAllocation::ArrayBytes<FVoxelTerrainGeometry>(Counts.MatchedRecords);
		const uint64 CaptureBytes = Predecessor && Predecessor->HasResourceBinding() ? 0 :
			VoxelBoundedAllocation::ArrayBytes<TSharedPtr<const FVoxelCoverageRecord, ESPMode::ThreadSafe>>(Counts.MatchedRecords) +
			VoxelBoundedAllocation::ArrayBytes<FVoxelCoverageRecordReadGuard>(Counts.MatchedRecords) +
			VoxelBoundedAllocation::ArrayBytes<FVoxelCoverageReadGuard>(Counts.GuardCount) +
			FVoxelCoverageLocalSnapshot::ArrayMetadataBytes();
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Input;
		if (Cache.Resources)
		{
			auto Consumer = Cache.Consumer;
			Consumer.Kind = Key.IsColumn() ? EVoxelGenerationConsumerKind::Heightfield : EVoxelGenerationConsumerKind::VoxelProxy;
			Consumer.SpatialKind = EVoxelGenerationSpatialKind::Bounds; Consumer.Min = Min; Consumer.Max = Max;
			if (!Cache.Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, InputBytes + CaptureBytes, Input))
			{ Error = TEXT("CapacityBlocked: coarse cache frozen inputs"); return false; }
			Out = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainStageCacheNode>(Input);
		}
		else Out = MakeShared<FVoxelTerrainStageCacheNode, ESPMode::ThreadSafe>();
		Out->Lease = MoveTemp(Input);
		Out->Sources.Reserve(SourceCount);
		for (const auto& Source : Sources)
			if (StageInfluences(Key, Min, Max, Source)) Out->Sources.Add(Source);
		Out->Sources.Sort([](const auto& A, const auto& B) { return StageCacheGuidBefore(A.Source.Id, B.Source.Id); });
		if (Predecessor)
		{
			FVoxelCoverageLocalSnapshot Local;
			if (!Predecessor->HasResourceBinding())
			{
				Local.ResourceLease = Out->Lease;
			}
			Predecessor->Capture(QueryMin, QueryMax, Local, false);
			if (Local.bCountOnly) { Error = TEXT("CapacityBlocked: coarse predecessor snapshot"); return false; }
			Out->Predecessors.Reserve(Counts.MatchedRecords);
			for (const auto& Record : Local.Records) Out->Predecessors.Add(Record->Geometry);
			Out->Predecessors.Sort(StageCacheGeometryBefore);
		}
		if (Out->Lease) Out->Lease->Resize(InputBytes);
		return true;
	}

	bool SameStageCacheInputs(const FVoxelTerrainStageCacheNode& A, const FVoxelTerrainStageCacheNode& B)
	{
		if (A.Sources.Num() != B.Sources.Num() || A.Predecessors != B.Predecessors) return false;
		for (int32 I = 0; I < A.Sources.Num(); ++I) if (!SameStageInfluence(A.Sources[I], B.Sources[I])) return false;
		return true;
	}

	bool ReplayStageCache(const FStageCacheNodePtr& Node, FVoxelTerrainStagePlan& Plan, FString& Error)
	{
		if (!Node) return true;
		if (Node->bPage)
		{
			if (!AppendStagePage(Plan, Node->Geometry, Node->Cost, Node->Priority, Node->bFloor, Error)) return false;
		}
		for (const auto& Child : Node->Children) if (!ReplayStageCache(Child, Plan, Error)) return false;
		return true;
	}
}
