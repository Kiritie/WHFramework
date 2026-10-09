// 私有缓存节点保存完整空间输入；共享子树只保留引用，变化路径在 Worker 重建。
struct FVoxelTerrainStageCacheNode
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	TArray<FVoxelTerrainStageSource> Sources;
	TArray<FVoxelCoverageRecord> Predecessors;
	TSharedPtr<const FVoxelTerrainStageCacheNode, ESPMode::ThreadSafe> Children[8];
	FVoxelTerrainGeometry Geometry;
	FVoxelTerrainPageCost Cost;
	int32 Priority = MAX_int32;
	bool bPage = false;
	bool bFloor = false;
	uint8 MaximumPageLevel = MAX_uint8, PolicyMaximumLevel = 0;
	int32 MaximumSamples = 0;
	double TargetPixels = 0.0;
	FVoxelTerrainPageCostLimits PolicyLimits;
	const FVoxelTerrainResources* AggregateLedger = nullptr;
	uint64 InputBytes = 0, MinimumSerial = MAX_uint64, MaximumSerial = 0;
	int32 LeaseCount = 0, PageCount = 0;
};

struct FVoxelTerrainStageSpatialNode
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	TSharedPtr<const FVoxelTerrainStageSpatialNode, ESPMode::ThreadSafe> Children[8];
	TSharedPtr<const FVoxelTerrainStageCacheNode, ESPMode::ThreadSafe> Value, FailureInputs;
	FVoxelTerrainSpatialKey Key;
	EVoxelTerrainResourceAdmission FailureAdmission = EVoxelTerrainResourceAdmission::None;
	uint64 WaitVersion = 0;
	FVoxelGenerationConsumerKey FailureConsumer;
	uint8 FailedMaximumPageLevel = MAX_uint8;
	const FVoxelTerrainResources* AggregateLedger = nullptr;
	uint64 InputBytes = 0, MinimumSerial = MAX_uint64, MaximumSerial = 0;
	int32 LeaseCount = 0, PageCount = 0, RetryRootCount = 0, FailedRootCount = 0;
};

struct FVoxelTerrainStageRootRange
{
	FIntVector Min = FIntVector::ZeroValue, Max = FIntVector::ZeroValue, Next = FIntVector::ZeroValue;
};
struct FVoxelTerrainStageRootRequest
{
	FVoxelTerrainSpatialKey Key;
	uint8 MaximumPageLevel = MAX_uint8;
};

struct FVoxelTerrainStageCacheIndex
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	TMap<FVoxelTerrainSpatialKey, TSharedPtr<const FVoxelTerrainStageCacheNode, ESPMode::ThreadSafe>> Roots;
	TSet<FVoxelTerrainSpatialKey> FailedRoots;
	TSharedPtr<const FVoxelTerrainStageSpatialNode, ESPMode::ThreadSafe> Slots[12];
	TSharedPtr<const FVoxelTerrainDomainSourceBatch, ESPMode::ThreadSafe> Sources;
	FVoxelCoverageIndex Predecessor;
	bool bIncremental = false;
	uint64 OutputVersion = 0, WorldEpoch = 0;
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

// 操作游标只派生自同一个 Source batch；Current 根直到整个事务完成才替换。
struct FVoxelTerrainStageWork
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	TSharedPtr<FVoxelTerrainStageCacheIndex, ESPMode::ThreadSafe> Candidate;
	TArray<FVoxelTerrainStageRootRange> Ranges;
	TArray<FVoxelTerrainStageRootRequest> Queue;
	TMap<FVoxelTerrainSpatialKey, uint8> Membership;
	int32 RangeCursor = 0, QueueCursor = 0, FailedRoots = 0;
	bool bTerminal = false, bInitialized = false, bRetryOnly = false, bOutputChanged = true;
};

namespace
{
	TAtomic<uint64> NextStageCommittedOutputVersion(0);
	using FStageCacheNodePtr = TSharedPtr<const FVoxelTerrainStageCacheNode, ESPMode::ThreadSafe>;

	void RefreshStageCacheAggregate(FVoxelTerrainStageCacheNode& Node)
	{
		// AggregateLedger 在节点首次授额时绑定原账本。
		Node.InputBytes = Node.Lease && Node.Lease->GetPool() == EVoxelTerrainResourcePool::Input ? Node.Lease->Bytes() : 0;
		Node.LeaseCount = Node.Lease.IsValid(); Node.PageCount = Node.bPage;
		Node.MinimumSerial = Node.Lease ? Node.Lease->Serial() : MAX_uint64;
		Node.MaximumSerial = Node.Lease ? Node.Lease->Serial() : 0;
		for (const auto& Child : Node.Children)
		{
			if (!Child) continue;
			if (Child->AggregateLedger != Node.AggregateLedger) Node.AggregateLedger = nullptr;
			Node.InputBytes += Child->InputBytes; Node.LeaseCount += Child->LeaseCount; Node.PageCount += Child->PageCount;
			Node.MinimumSerial = FMath::Min(Node.MinimumSerial, Child->MinimumSerial);
			Node.MaximumSerial = FMath::Max(Node.MaximumSerial, Child->MaximumSerial);
		}
	}
	struct FStageNativeCapacityScope
	{
		FStageNativeCapacityScope(FVoxelTerrainStageCache* InCache, FVoxelTerrainStageCacheNode* InNode, const FVoxelTerrainSpatialKey& InKey);
		FStageNativeCapacityScope(FVoxelTerrainStageCache* InCache, const FVoxelTerrainStageCacheNode* InNode, const FVoxelTerrainSpatialKey& InKey);
		~FStageNativeCapacityScope();
		FVoxelTerrainStageCache* Cache;
		FVoxelTerrainStageCacheNode* Node;
		const FVoxelTerrainStageCacheNode* RetainedNode;
		FVoxelTerrainSpatialKey Key;
		FStageNativeCapacityScope* Previous;
	};
	thread_local FStageNativeCapacityScope* StageNativeCapacityScope = nullptr;
	FStageNativeCapacityScope::FStageNativeCapacityScope(FVoxelTerrainStageCache* InCache, FVoxelTerrainStageCacheNode* InNode, const FVoxelTerrainSpatialKey& InKey)
		: Cache(InCache), Node(InNode), RetainedNode(InNode), Key(InKey), Previous(StageNativeCapacityScope)
	{ StageNativeCapacityScope = this; }
	FStageNativeCapacityScope::FStageNativeCapacityScope(FVoxelTerrainStageCache* InCache, const FVoxelTerrainStageCacheNode* InNode, const FVoxelTerrainSpatialKey& InKey)
		: Cache(InCache), Node(nullptr), RetainedNode(InNode), Key(InKey), Previous(StageNativeCapacityScope)
	{ StageNativeCapacityScope = this; }
	FStageNativeCapacityScope::~FStageNativeCapacityScope()
	{
		if (Node) RefreshStageCacheAggregate(*Node);
		StageNativeCapacityScope = Previous;
	}
	int32 StageSpatialRootSlot(const FVoxelTerrainSpatialKey& Key)
	{
		return (Key.IsColumn() ? 8 : 0) + (Key.Coordinate.X >= 0 ? 1 : 0) +
			(Key.Coordinate.Y >= 0 ? 2 : 0) + (!Key.IsColumn() && Key.Coordinate.Z >= 0 ? 4 : 0);
	}
	int32 StageSpatialChildSlot(const FVoxelTerrainSpatialKey& Key, const uint8 Level)
	{
		const uint32 Shift = Level - Key.Level - 1;
		return ((uint32(Key.Coordinate.X) >> Shift) & 1) | (((uint32(Key.Coordinate.Y) >> Shift) & 1) << 1) |
			(Key.IsColumn() ? 0 : (((uint32(Key.Coordinate.Z) >> Shift) & 1) << 2));
	}
	FStageCacheNodePtr FindStageSpatialRoot(const FVoxelTerrainStageCacheIndex& Index, const FVoxelTerrainSpatialKey& Key)
	{
		const auto* Node = Index.Slots[StageSpatialRootSlot(Key)].Get();
		for (uint8 Level = 27; Node && Level > Key.Level; --Level)
			Node = Node->Children[StageSpatialChildSlot(Key, Level)].Get();
		return Node ? Node->Value : nullptr;
	}
	const FVoxelTerrainStageSpatialNode* FindStageSpatialEntry(const FVoxelTerrainStageCacheIndex& Index, const FVoxelTerrainSpatialKey& Key)
	{
		const auto* Node = Index.Slots[StageSpatialRootSlot(Key)].Get();
		for (uint8 Level = 27; Node && Level > Key.Level; --Level) Node = Node->Children[StageSpatialChildSlot(Key, Level)].Get();
		return Node;
	}
	void SumStageCacheUnion(const FVoxelTerrainStageCacheNode* Left, const FVoxelTerrainStageCacheNode* Right,
		const FVoxelTerrainResources* Resources, uint64& Bytes, int32& Count, uint64& Minimum, uint64& Maximum)
	{
		if (!Left && !Right) return;
		const auto* Shared = Left == Right ? Left : !Left ? Right : !Right ? Left : nullptr;
		if (Shared && Shared->AggregateLedger == Resources)
		{
			Bytes += Shared->InputBytes; Count += Shared->LeaseCount;
			Minimum = FMath::Min(Minimum, Shared->MinimumSerial); Maximum = FMath::Max(Maximum, Shared->MaximumSerial); return;
		}
		const FVoxelTerrainStageCacheNode* Distinct[] = {Left, Right != Left ? Right : nullptr};
		for (const auto* Node : Distinct)
		{
			if (!Node || !Node->Lease || !Resources || !Node->Lease->IsOwnedBy(*Resources)) continue;
			Bytes += Node->Lease->GetPool() == EVoxelTerrainResourcePool::Input ? Node->Lease->Bytes() : 0; ++Count;
			Minimum = FMath::Min(Minimum, Node->Lease->Serial()); Maximum = FMath::Max(Maximum, Node->Lease->Serial());
		}
		for (int32 Child = 0; Child < 8; ++Child) SumStageCacheUnion(Left ? Left->Children[Child].Get() : nullptr,
			Right ? Right->Children[Child].Get() : nullptr, Resources, Bytes, Count, Minimum, Maximum);
	}
	void RefreshStageSpatialAggregate(FVoxelTerrainStageSpatialNode& Node, const FVoxelTerrainResources* Resources)
	{
		Node.AggregateLedger = Resources;
		Node.InputBytes = Node.Lease && Resources && Node.Lease->IsOwnedBy(*Resources) && Node.Lease->GetPool() == EVoxelTerrainResourcePool::Input ? Node.Lease->Bytes() : 0;
		Node.LeaseCount = Node.Lease && Resources && Node.Lease->IsOwnedBy(*Resources);
		Node.RetryRootCount = Node.FailureAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked;
		Node.FailedRootCount = Node.FailureAdmission != EVoxelTerrainResourceAdmission::None;
		Node.PageCount = 0; Node.MinimumSerial = Node.Lease ? Node.Lease->Serial() : MAX_uint64; Node.MaximumSerial = Node.Lease ? Node.Lease->Serial() : 0;
		const auto Add = [&Node](const auto& Part)
		{
			if (!Part) return;
			if (Part->AggregateLedger != Node.AggregateLedger) Node.AggregateLedger = nullptr;
			Node.InputBytes += Part->InputBytes; Node.LeaseCount += Part->LeaseCount; Node.PageCount += Part->PageCount;
			Node.MinimumSerial = FMath::Min(Node.MinimumSerial, Part->MinimumSerial); Node.MaximumSerial = FMath::Max(Node.MaximumSerial, Part->MaximumSerial);
		};
		SumStageCacheUnion(Node.Value.Get(), Node.FailureInputs.Get(), Resources, Node.InputBytes, Node.LeaseCount, Node.MinimumSerial, Node.MaximumSerial);
		if ((Node.Value && Node.Value->AggregateLedger != Resources) || (Node.FailureInputs && Node.FailureInputs->AggregateLedger != Resources)) Node.AggregateLedger = nullptr;
		Node.PageCount = Node.Value ? Node.Value->PageCount : 0;
		for (const auto& Child : Node.Children)
		{ if (Child) { Node.RetryRootCount += Child->RetryRootCount; Node.FailedRootCount += Child->FailedRootCount; } Add(Child); }
	}
	bool StagePlanRetainedFloor(const FVoxelTerrainStagePlan& Plan, uint64& Input, int32& Count)
	{
		Input = Plan.RetainedInputBytes; Count = Plan.RetainedLeaseCount;
		return !Plan.CapacityCache || !Plan.Resources || Plan.CapacityCache->AccumulateRetainedInput(*Plan.Resources, nullptr, {}, Input, Count, !Plan.CapacityCache->bExternalCoverageFloor);
	}

	struct FStageRetainedFloor
	{
		void Add(const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease, const FVoxelTerrainResources* Resources)
		{
			if (!Resources || !Lease || !Lease->IsOwnedBy(*Resources)) return;
			for (int32 Index = 0; Index < Count; ++Index) if (Seen[Index] == Lease.Get()) return;
			check(Count < UE_ARRAY_COUNT(Seen));
			Seen[Count++] = Lease.Get();
			if (Lease->GetPool() == EVoxelTerrainResourcePool::Input) Input += Lease->Bytes();
			if (Lease->GetPool() == EVoxelTerrainResourcePool::Working) Working += Lease->Bytes();
		}
		void Bind(FVoxelTerrainStagePlan& Plan) const
		{
			Plan.RetainedInputBytes = Input;
			Plan.RetainedWorkingBytes = Working;
			Plan.RetainedLeaseCount = Count;
		}
		uint64 Input = 0, Working = 0;
		int32 Count = 0;
		const FVoxelTerrainResourceLease* Seen[5] = {};
	};

	bool StageCapacityPeak(const uint64* OldParts, const uint64* NewParts, const bool* Growing, const int32 Count,
		uint64& OutPeak, uint64& OutBytes, FString& Error)
	{
		OutPeak = OutBytes = 0;
		uint64 Current = 0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (NewParts[Index] == MAX_uint64 || NewParts[Index] > MAX_uint64 - OutBytes || OldParts[Index] > MAX_uint64 - Current)
			{ Error = TEXT("CapacityUnachievable: coarse native allocator capacity"); return false; }
			OutBytes += NewParts[Index];
			Current += OldParts[Index];
		}
		OutPeak = Current;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (!Growing[Index]) continue;
			if (NewParts[Index] > MAX_uint64 - Current)
			{ Error = TEXT("CapacityUnachievable: coarse native allocator peak"); return false; }
			// 各容器顺序 Reserve；之前已释放的旧容器不叠加到后续峰值。
			OutPeak = FMath::Max(OutPeak, Current + NewParts[Index]);
			Current = Current - OldParts[Index] + NewParts[Index];
		}
		return true;
	}

	bool ReserveStagePlan(FVoxelTerrainStagePlan& Plan, const int32 Required, FString& Error)
	{
		if (Required < 0 || Required > (1 << 20)) { Error = TEXT("CapacityUnachievable: coarse plan metadata"); return false; }
		if (Required <= Plan.ReservedPages) return true;
		const int32 Capacity = FMath::Max(Required, FMath::Min(1 << 20, FMath::Max(8, Plan.ReservedPages * 2)));
		const uint64 OldParts[] = {VoxelGenerationAllocation::AllocatedBytes(Plan.Pages),
			VoxelGenerationAllocation::AllocatedBytes(Plan.Costs), VoxelGenerationAllocation::AllocatedBytes(Plan.Priorities),
			VoxelGenerationAllocation::AllocatedBytes(Plan.ResolutionFloorPages)};
		const uint64 NewParts[] = {
			VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FVoxelTerrainSpatialKey, FVoxelTerrainGeometry>>(Plan.Pages, Capacity),
			VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FVoxelTerrainSpatialKey, FVoxelTerrainPageCost>>(Plan.Costs, Capacity),
			VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FVoxelTerrainSpatialKey, int32>>(Plan.Priorities, Capacity),
			VoxelGenerationAllocation::Private::SetReserveBytes<FVoxelTerrainSpatialKey>(Plan.ResolutionFloorPages, Capacity)};
		const bool Growing[] = {
			!VoxelGenerationAllocation::Private::SetCanReserve<TPair<FVoxelTerrainSpatialKey, FVoxelTerrainGeometry>>(Plan.Pages, Capacity),
			!VoxelGenerationAllocation::Private::SetCanReserve<TPair<FVoxelTerrainSpatialKey, FVoxelTerrainPageCost>>(Plan.Costs, Capacity),
			!VoxelGenerationAllocation::Private::SetCanReserve<TPair<FVoxelTerrainSpatialKey, int32>>(Plan.Priorities, Capacity),
			!VoxelGenerationAllocation::Private::SetCanReserve<FVoxelTerrainSpatialKey>(Plan.ResolutionFloorPages, Capacity)};
		uint64 Peak = 0, Bytes = 0;
		if (!StageCapacityPeak(OldParts, NewParts, Growing, UE_ARRAY_COUNT(OldParts), Peak, Bytes, Error)) return false;
		if (Plan.Resources)
		{
			const uint64 Limit = Plan.Resources->Snapshot().Limits.WorkingBytes;
			if (Peak > Limit || Plan.RetainedWorkingBytes > Limit - Peak)
			{ Error = TEXT("CapacityUnachievable: coarse plan retained Working peak"); return false; }
			uint64 RequiredInput = 0; int32 RequiredCount = 0;
			if (!StagePlanRetainedFloor(Plan, RequiredInput, RequiredCount))
			{ Error = TEXT("CapacityUnachievable: coarse strong input floor"); return false; }
			const auto Admission = Plan.ResourceLease ? Plan.ResourceLease->TryResize(FMath::Max<uint64>(1, Peak), RequiredInput, RequiredCount) :
				Plan.Resources->TryAcquire(Plan.Consumer, EVoxelTerrainResourcePool::Working, FMath::Max<uint64>(1, Peak), Plan.ResourceLease,
					0, RequiredInput, RequiredCount);
			if (Admission != EVoxelTerrainResourceAdmission::Granted)
			{
				Error = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: coarse plan containers") :
					TEXT("CapacityUnachievable: coarse plan growth peak");
				return false;
			}
		}
		Plan.Pages.Reserve(Capacity); Plan.Costs.Reserve(Capacity);
		Plan.Priorities.Reserve(Capacity); Plan.ResolutionFloorPages.Reserve(Capacity);
		Plan.ReservedPages = Capacity;
		const uint64 Actual = VoxelGenerationAllocation::AllocatedBytes(Plan.Pages) + VoxelGenerationAllocation::AllocatedBytes(Plan.Costs) +
			VoxelGenerationAllocation::AllocatedBytes(Plan.Priorities) + VoxelGenerationAllocation::AllocatedBytes(Plan.ResolutionFloorPages);
		check(Actual <= Bytes);
		if (Plan.ResourceLease) verify(Plan.ResourceLease->Resize(FMath::Max<uint64>(1, Actual)));
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
		const uint64 OldBytes = VoxelGenerationAllocation::AllocatedBytes(Array);
		const uint64 Bytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<ElementType, FDefaultAllocator>(Capacity);
		if (OldBytes > MAX_uint64 - OtherBytes || Bytes > MAX_uint64 - OtherBytes - OldBytes)
		{ Error = TEXT("CapacityUnachievable: coarse scratch allocator capacity"); return false; }
		if (Plan.Resources)
		{
			const uint64 Peak = OtherBytes + OldBytes + Bytes;
			const bool bOwnPlan = Plan.ResourceLease && Plan.ResourceLease->IsOwnedBy(*Plan.Resources);
			const uint64 PlanWorking = bOwnPlan && Plan.ResourceLease->GetPool() == EVoxelTerrainResourcePool::Working ? Plan.ResourceLease->Bytes() : 0;
			const uint64 Limit = Plan.Resources->Snapshot().Limits.WorkingBytes;
			if (Peak > Limit || PlanWorking > Limit - Peak || Plan.RetainedWorkingBytes > Limit - Peak - PlanWorking)
			{ Error = TEXT("CapacityUnachievable: coarse scratch retained Working peak"); return false; }
			uint64 RequiredInput = 0; int32 RequiredCount = 0;
			if (!StagePlanRetainedFloor(Plan, RequiredInput, RequiredCount))
			{ Error = TEXT("CapacityUnachievable: coarse scratch strong input floor"); return false; }
			const auto Admission = Lease ? Lease->TryResize(Peak, RequiredInput, RequiredCount + int32(bOwnPlan)) :
				Plan.Resources->TryAcquire(Plan.Consumer, EVoxelTerrainResourcePool::Working, Peak, Lease,
					0, RequiredInput, RequiredCount + int32(bOwnPlan));
			if (Admission != EVoxelTerrainResourceAdmission::Granted)
			{
				Error = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: coarse scratch") :
					TEXT("CapacityUnachievable: coarse scratch peak");
				return false;
			}
		}
		Array.Reserve(Capacity);
		const uint64 Actual = VoxelGenerationAllocation::AllocatedBytes(Array);
		check(Actual <= Bytes);
		if (Lease) verify(Lease->Resize(OtherBytes + Actual));
		return true;
	}

	bool SameStageInfluence(const FVoxelTerrainStageSource& A, const FVoxelTerrainStageSource& B)
	{
		return A.Source.Id == B.Source.Id && A.Source.Center == B.Source.Center &&
			A.Source.VerticalFovDegrees == B.Source.VerticalFovDegrees && A.Source.ViewportHeightPixels == B.Source.ViewportHeightPixels &&
			A.Source.GetSchedulingPriority() == B.Source.GetSchedulingPriority() &&
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
		FVoxelTerrainStageCache& Cache, FVoxelTerrainStagePlan& Plan, TSharedPtr<FVoxelTerrainStageCacheNode, ESPMode::ThreadSafe>& Out, FString& Error)
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
			VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelTerrainStageSource, FDefaultAllocator>(SourceCount) +
			VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelCoverageRecord, FDefaultAllocator>(Counts.MatchedRecords);
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Input;
		if (Cache.Resources)
		{
			const bool bOwnPlan = Plan.ResourceLease && Plan.ResourceLease->IsOwnedBy(*Cache.Resources);
			uint64 RequiredInput = 0; int32 RequiredCount = 0;
			if (!StagePlanRetainedFloor(Plan, RequiredInput, RequiredCount))
			{ Error = TEXT("CapacityUnachievable: coarse cache strong input floor"); return false; }
			const auto Admission = Cache.Resources->TryAcquire(Cache.Consumer, EVoxelTerrainResourcePool::Input, InputBytes, Input,
				0, RequiredInput, RequiredCount + int32(bOwnPlan));
			if (Admission != EVoxelTerrainResourceAdmission::Granted)
			{
				Error = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: coarse cache frozen inputs") :
					TEXT("CapacityUnachievable: coarse cache frozen inputs");
				return false;
			}
			Out = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainStageCacheNode>(Input);
		}
		else Out = MakeShared<FVoxelTerrainStageCacheNode, ESPMode::ThreadSafe>();
		Out->Lease = MoveTemp(Input);
		Out->AggregateLedger = Cache.Resources.Get();
		Out->Sources.Reserve(SourceCount);
		for (const auto& Source : Sources)
			if (StageInfluences(Key, Min, Max, Source)) Out->Sources.Add(Source);
		Out->Sources.Sort([](const auto& A, const auto& B) { return StageCacheGuidBefore(A.Source.Id, B.Source.Id); });
		if (Predecessor)
		{
			Out->Predecessors.Reserve(Counts.MatchedRecords);
			Predecessor->AnyRecord(QueryMin, QueryMax, [&](const FVoxelCoverageRecord& Record)
			{ Out->Predecessors.Add(Record); return false; });
			Out->Predecessors.Sort([](const auto& Left, const auto& Right)
			{ return StageCacheGeometryBefore(Left.Geometry, Right.Geometry); });
		}
		const uint64 Actual = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainStageCacheNode>() +
			VoxelGenerationAllocation::AllocatedBytes(Out->Sources) + VoxelGenerationAllocation::AllocatedBytes(Out->Predecessors);
		check(Actual <= InputBytes);
		if (Out->Lease) verify(Out->Lease->Resize(Actual));
		RefreshStageCacheAggregate(*Out);
		return true;
	}

	bool SameStageCacheInputs(const FVoxelTerrainStageCacheNode& A, const FVoxelTerrainStageCacheNode& B)
	{
		if (A.PolicyMaximumLevel != B.PolicyMaximumLevel || A.MaximumSamples != B.MaximumSamples || A.TargetPixels != B.TargetPixels ||
			A.PolicyLimits.Triangles != B.PolicyLimits.Triangles || A.PolicyLimits.MeshBytes != B.PolicyLimits.MeshBytes ||
			A.PolicyLimits.BuildMilliseconds != B.PolicyLimits.BuildMilliseconds ||
			A.Geometry.WorldMinZ != B.Geometry.WorldMinZ || A.Geometry.WorldMaxZ != B.Geometry.WorldMaxZ ||
			A.Geometry.GeneratorIdentity != B.Geometry.GeneratorIdentity || A.Geometry.MaterialRevision != B.Geometry.MaterialRevision) return false;
		if (A.Sources.Num() != B.Sources.Num() || A.Predecessors.Num() != B.Predecessors.Num() || A.MaximumPageLevel != B.MaximumPageLevel) return false;
		for (int32 I = 0; I < A.Predecessors.Num(); ++I)
			if (A.Predecessors[I].Geometry != B.Predecessors[I].Geometry || A.Predecessors[I].Stamp != B.Predecessors[I].Stamp ||
				A.Predecessors[I].bWanted != B.Predecessors[I].bWanted || A.Predecessors[I].bReady != B.Predecessors[I].bReady ||
				A.Predecessors[I].bHasGeometry != B.Predecessors[I].bHasGeometry) return false;
		for (int32 I = 0; I < A.Sources.Num(); ++I) if (!SameStageInfluence(A.Sources[I], B.Sources[I])) return false;
		return true;
	}

	bool StageFailureInputsCurrent(const FVoxelTerrainStageCacheNode& Goal, const FVoxelTerrainSpatialKey& Key,
		TConstArrayView<FVoxelTerrainStageSource> Sources, const FVoxelCoverageIndex& Predecessor, const uint8 MaximumLevel,
		const int32 Samples, const int32 MinZ, const int32 MaxZ, const double Pixels, const uint64 Generator,
		const uint64 Material, const FVoxelTerrainPageCostLimits& Limits, const uint8 MaximumPageLevel,
		const FVoxelTerrainResources* Resources)
	{
		if ((Resources && Goal.Lease && !Goal.Lease->IsOwnedBy(*Resources)) || Goal.Geometry.Key != Key ||
			Goal.PolicyMaximumLevel != MaximumLevel || Goal.MaximumSamples != Samples || Goal.TargetPixels != Pixels ||
			Goal.Geometry.WorldMinZ != MinZ || Goal.Geometry.WorldMaxZ != MaxZ || Goal.Geometry.GeneratorIdentity != Generator ||
			Goal.Geometry.MaterialRevision != Material || Goal.MaximumPageLevel != MaximumPageLevel ||
			Goal.PolicyLimits.Triangles != Limits.Triangles || Goal.PolicyLimits.MeshBytes != Limits.MeshBytes || Goal.PolicyLimits.BuildMilliseconds != Limits.BuildMilliseconds) return false;
		FIntVector Min, Max;
		if (!Key.TryBounds(MinZ, MaxZ, Min, Max)) return false;
		int32 SourceCount = 0;
		for (const auto& Source : Sources)
		{
			if (!StageInfluences(Key, Min, Max, Source)) continue;
			++SourceCount;
			const auto* Old = Goal.Sources.FindByPredicate([&](const auto& Entry) { return Entry.Source.Id == Source.Source.Id; });
			if (!Old || !SameStageInfluence(*Old, Source)) return false;
		}
		if (SourceCount != Goal.Sources.Num()) return false;
		FIntVector QueryMin, QueryMax;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{ QueryMin[Axis] = int32(FMath::Max<int64>(MIN_int32, int64(Min[Axis]) - 1)); QueryMax[Axis] = int32(FMath::Min<int64>(MAX_int32, int64(Max[Axis]) + 1)); }
		FVoxelCoverageLocalSnapshot Counts;
		Predecessor.CountCapture(QueryMin, QueryMax, Counts);
		if (Counts.MatchedRecords != Goal.Predecessors.Num()) return false;
		bool Same = true;
		Predecessor.AnyRecord(QueryMin, QueryMax, [&](const FVoxelCoverageRecord& Record)
		{
			const auto* Old = Goal.Predecessors.FindByPredicate([&](const auto& Entry) { return Entry.Geometry.Key == Record.Geometry.Key; });
			Same = Old && Old->Geometry == Record.Geometry && Old->Stamp == Record.Stamp && Old->bWanted == Record.bWanted &&
				Old->bReady == Record.bReady && Old->bHasGeometry == Record.bHasGeometry;
			return !Same;
		});
		return Same;
	}
	template<typename VisitorType>
	void VisitStageFailedRoots(const FVoxelTerrainStageCacheIndex& Index, VisitorType&& Visitor)
	{
		const auto Visit = [&](auto&& Self, const FVoxelTerrainStageSpatialNode* Node) -> void
		{
			if (!Node || !Node->FailedRootCount) return;
			if (Node->FailureAdmission != EVoxelTerrainResourceAdmission::None) Visitor(*Node);
			for (const auto& Child : Node->Children) Self(Self, Child.Get());
		};
		for (const auto& Slot : Index.Slots) Visit(Visit, Slot.Get());
	}
	bool ReserveStageWork(FVoxelTerrainStageWork& Work, const int32 RangeCapacity, const int32 QueueCapacity,
		const int32 MembershipCapacity, FVoxelTerrainStagePlan& Plan, FString& Error)
	{
		if (RangeCapacity < 0 || QueueCapacity < 0 || MembershipCapacity < 0 || RangeCapacity > (1 << 20) ||
			QueueCapacity > (1 << 22) || MembershipCapacity > (1 << 20))
		{ Error = TEXT("CapacityUnachievable: coarse dirty root metadata"); return false; }
		const uint64 Old[] = {VoxelGenerationAllocation::AllocatedBytes(Work.Ranges), VoxelGenerationAllocation::AllocatedBytes(Work.Queue),
			VoxelGenerationAllocation::AllocatedBytes(Work.Membership)};
		const uint64 New[] = {
			FMath::Max(Old[0], VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelTerrainStageRootRange, FDefaultAllocator>(RangeCapacity)),
			FMath::Max(Old[1], VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelTerrainStageRootRequest, FDefaultAllocator>(QueueCapacity)),
			VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FVoxelTerrainSpatialKey, uint8>>(Work.Membership, MembershipCapacity)};
		const bool Growing[] = {Work.Ranges.Max() < RangeCapacity, Work.Queue.Max() < QueueCapacity,
			!VoxelGenerationAllocation::Private::SetCanReserve<TPair<FVoxelTerrainSpatialKey, uint8>>(Work.Membership, MembershipCapacity)};
		if (!Growing[0] && !Growing[1] && !Growing[2] && Work.Lease) return true;
		uint64 Peak = 0, Bytes = 0;
		if (!StageCapacityPeak(Old, New, Growing, UE_ARRAY_COUNT(Old), Peak, Bytes, Error)) return false;
		const uint64 Metadata = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainStageWork>();
		if (Peak > MAX_uint64 - Metadata) { Error = TEXT("CapacityUnachievable: coarse dirty metadata peak"); return false; }
		if (Plan.Resources)
		{
			uint64 Input = 0; int32 Count = 0;
			const uint64 Excluded[] = {Work.Lease ? Work.Lease->Serial() : 0};
			Input = Plan.RetainedInputBytes; Count = Plan.RetainedLeaseCount;
			if (Plan.CapacityCache && !Plan.CapacityCache->AccumulateRetainedInput(*Plan.Resources, nullptr, MakeArrayView(Excluded), Input, Count, !Plan.CapacityCache->bExternalCoverageFloor))
			{ Error = TEXT("CapacityUnachievable: coarse dirty work retained input"); return false; }
			Count += int32(Plan.ResourceLease && Plan.ResourceLease->IsOwnedBy(*Plan.Resources));
			const auto Admission = Work.Lease ? Work.Lease->TryResize(Metadata + Peak, Input, Count) :
				Plan.Resources->TryAcquire(Plan.Consumer, EVoxelTerrainResourcePool::Input, Metadata + Peak, Work.Lease, 0, Input, Count);
			if (Admission != EVoxelTerrainResourceAdmission::Granted)
			{ Error = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: coarse dirty work") : TEXT("CapacityUnachievable: coarse dirty work"); return false; }
		}
		Work.Ranges.Reserve(RangeCapacity); Work.Queue.Reserve(QueueCapacity); Work.Membership.Reserve(MembershipCapacity);
		const uint64 Actual = Metadata + VoxelGenerationAllocation::AllocatedBytes(Work.Ranges) + VoxelGenerationAllocation::AllocatedBytes(Work.Queue) +
			VoxelGenerationAllocation::AllocatedBytes(Work.Membership);
		check(Actual <= Metadata + Bytes);
		if (Work.Lease) verify(Work.Lease->Resize(Actual));
		return true;
	}
	bool EnqueueStageRoot(FVoxelTerrainStageWork& Work, const FVoxelTerrainSpatialKey& Key, const uint8 MaximumPageLevel,
		FVoxelTerrainStagePlan& Plan, FString& Error)
	{
		if (const auto* Previous = Work.Membership.Find(Key); Previous && *Previous <= MaximumPageLevel) return true;
		const int32 Membership = Work.Membership.Num() + (Work.Membership.Contains(Key) ? 0 : 1);
		const int32 QueueCapacity = FMath::Max(Work.Queue.Num() + 1, FMath::Min(1 << 22, FMath::Max(16, Work.Queue.Max() * 2)));
		const int32 MembershipCapacity = FMath::Max(Membership, FMath::Min(1 << 20, FMath::Max(16, Work.Membership.Num() * 2)));
		if (!ReserveStageWork(Work, Work.Ranges.Max(), QueueCapacity, MembershipCapacity, Plan, Error)) return false;
		Work.Membership.Add(Key, MaximumPageLevel); Work.Queue.Add({Key, MaximumPageLevel}); return true;
	}
	bool StageRangeForBounds(const FIntVector& Min, const FIntVector& Max, const bool Column, const uint8 Level,
		FVoxelTerrainStageRootRange& Out)
	{
		const int64 Side = int64(16) << Level;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (Column && Axis == 2) { Out.Min.Z = Out.Max.Z = 0; continue; }
			if (Max[Axis] <= Min[Axis]) return false;
			Out.Min[Axis] = int32(VoxelStreamingGeometry::FloorDivide(Min[Axis], Side));
			Out.Max[Axis] = int32(VoxelStreamingGeometry::FloorDivide(int64(Max[Axis]) - 1, Side));
		}
		Out.Next = Out.Min; return true;
	}
	bool StageSourceRootRange(const FVoxelTerrainStageSource& Source, const bool Column, const uint8 Level,
		const int32 MinZ, const int32 MaxZ, FVoxelTerrainStageRootRange& Out)
	{
		if (Source.OuterRadiusCells <= 0 || (!Column && Source.VerticalHalfExtentCells <= 0)) return false;
		FIntVector Min, Max;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const int64 Radius = Axis == 2 ? Source.VerticalHalfExtentCells : Source.OuterRadiusCells;
			Min[Axis] = int32(FMath::Max<int64>(MIN_int32, int64(Source.Source.Center[Axis]) - Radius));
			Max[Axis] = int32(FMath::Min<int64>(MAX_int32, int64(Source.Source.Center[Axis]) + Radius + 1));
		}
		Min.Z = FMath::Max(Min.Z, MinZ); Max.Z = FMath::Min(Max.Z, MaxZ);
		return StageRangeForBounds(Min, Max, Column, Level, Out);
	}
	bool NextStageRangeRoot(FVoxelTerrainStageWork& Work, const EVoxelTerrainRepresentation Representation,
		const uint8 Level, FVoxelTerrainSpatialKey& Out)
	{
		if (!Work.Ranges.IsValidIndex(Work.RangeCursor)) return false;
		auto& Range = Work.Ranges[Work.RangeCursor];
		Out = {Representation, Range.Next, Level};
		if (++Range.Next.X > Range.Max.X)
		{
			Range.Next.X = Range.Min.X;
			if (++Range.Next.Y > Range.Max.Y)
			{
				Range.Next.Y = Range.Min.Y;
				if (++Range.Next.Z > Range.Max.Z) ++Work.RangeCursor;
			}
		}
		return true;
	}
	bool CommitStageSpatialRoot(FVoxelTerrainStageCache& Cache, FVoxelTerrainStageWork& Work,
		const FVoxelTerrainSpatialKey& Key, FStageCacheNodePtr Value, FVoxelTerrainStagePlan& Plan, FString& Error,
		FStageCacheNodePtr FailureInputs = nullptr, EVoxelTerrainResourceAdmission FailureAdmission = EVoxelTerrainResourceAdmission::None,
		uint64 WaitVersion = 0, uint8 FailedMaximumPageLevel = MAX_uint8)
	{
		const int32 PathCount = 28 - Key.Level;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Grant;
		if (Cache.Resources)
		{
			uint64 Input = 0; int32 Count = 0;
			if (!StagePlanRetainedFloor(Plan, Input, Count)) { Error = TEXT("CapacityUnachievable: coarse changed index floor"); return false; }
			// 新根尚未进入索引；NativeScope 将旧新子树沿同空间路径去重。
			const auto Admission = Cache.Resources->TryAcquire(Cache.Consumer, EVoxelTerrainResourcePool::Input,
				uint64(PathCount) * VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainStageSpatialNode>(), Grant,
				PathCount, Input, Count + int32(Plan.ResourceLease.IsValid()));
			if (Admission != EVoxelTerrainResourceAdmission::Granted)
			{ Error = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: coarse changed index path") : TEXT("CapacityUnachievable: coarse changed index path"); return false; }
		}
		const auto Mutate = [&](auto&& Self, TSharedPtr<const FVoxelTerrainStageSpatialNode, ESPMode::ThreadSafe> Old,
			const uint8 Level) -> TSharedPtr<const FVoxelTerrainStageSpatialNode, ESPMode::ThreadSafe>
		{
			const auto Lease = Grant ? Grant->Split(VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainStageSpatialNode>()) : nullptr;
			auto Node = Lease ? VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainStageSpatialNode>(Lease) : MakeShared<FVoxelTerrainStageSpatialNode, ESPMode::ThreadSafe>();
			if (Old) *Node = *Old;
			Node->Lease = Lease;
			if (Level == Key.Level)
			{ Node->Value = Value; Node->FailureInputs = FailureInputs; Node->FailureAdmission = FailureAdmission;
				Node->WaitVersion = WaitVersion; Node->FailureConsumer = Cache.Consumer; Node->FailedMaximumPageLevel = FailedMaximumPageLevel; Node->Key = Key; }
			else
			{
				const int32 Child = StageSpatialChildSlot(Key, Level);
				Node->Children[Child] = Self(Self, Node->Children[Child], uint8(Level - 1));
			}
			RefreshStageSpatialAggregate(*Node, Cache.Resources.Get());
			return (Node->PageCount || Node->FailedRootCount) ? TSharedPtr<const FVoxelTerrainStageSpatialNode, ESPMode::ThreadSafe>(Node) : nullptr;
		};
		const int32 Slot = StageSpatialRootSlot(Key);
		Work.Candidate->Slots[Slot] = Mutate(Mutate, Work.Candidate->Slots[Slot], 27);
		return true;
	}
	const FVoxelTerrainStageCacheNode* FindStagePageAt(const FVoxelTerrainStageCacheIndex& Index,
		const EVoxelTerrainRepresentation Representation, const FIntVector& Point)
	{
		const int64 Side = int64(16) << Index.MaximumLevel;
		const FVoxelTerrainSpatialKey Root{Representation, FIntVector(int32(VoxelStreamingGeometry::FloorDivide(Point.X, Side)),
			int32(VoxelStreamingGeometry::FloorDivide(Point.Y, Side)), Representation == EVoxelTerrainRepresentation::VoxelProxy ?
			int32(VoxelStreamingGeometry::FloorDivide(Point.Z, Side)) : 0), Index.MaximumLevel};
		const auto Found = FindStageSpatialRoot(Index, Root);
		const auto* Node = Found.Get();
		for (uint8 Level = Index.MaximumLevel; Node && !Node->bPage && Level > 0; --Level)
		{
			const int64 ChildSide = int64(16) << (Level - 1);
			const int32 Child = (uint32(VoxelStreamingGeometry::FloorDivide(Point.X, ChildSide)) & 1) |
				((uint32(VoxelStreamingGeometry::FloorDivide(Point.Y, ChildSide)) & 1) << 1) |
				(Representation == EVoxelTerrainRepresentation::VoxelProxy ? ((uint32(VoxelStreamingGeometry::FloorDivide(Point.Z, ChildSide)) & 1) << 2) : 0);
			Node = Node->Children[Child].Get();
		}
		return Node && Node->bPage ? Node : nullptr;
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
