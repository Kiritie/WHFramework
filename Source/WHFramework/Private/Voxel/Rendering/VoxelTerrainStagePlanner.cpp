#include "Voxel/Rendering/VoxelTerrainStagePlanner.h"
#include "Voxel/Rendering/VoxelCoverageIndex.h"
#include "Voxel/Rendering/VoxelCoveragePartition.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Misc/ScopeExit.h"

#include "VoxelTerrainStageCache.inl"

FVoxelTerrainStagePlan::FVoxelTerrainStagePlan() = default;

FVoxelTerrainStagePlan::FVoxelTerrainStagePlan(FVoxelTerrainStagePlan&& InOther)
{
	*this = MoveTemp(InOther);
}

FVoxelTerrainStagePlan& FVoxelTerrainStagePlan::operator=(FVoxelTerrainStagePlan&& InOther)
{
	if (this != &InOther)
	{
		// 四个旧容器先销毁 backing，再释放其额度；移动不创建新的 native backing。
		Pages = MoveTemp(InOther.Pages);
		Priorities = MoveTemp(InOther.Priorities);
		ResolutionFloorPages = MoveTemp(InOther.ResolutionFloorPages);
		Costs = MoveTemp(InOther.Costs);
		ResourceLease = MoveTemp(InOther.ResourceLease);
		Resources = MoveTemp(InOther.Resources);
		Consumer = InOther.Consumer;
		CapacityCache = InOther.CapacityCache;
		InOther.CapacityCache = nullptr;
		ReservedPages = InOther.ReservedPages;
		FailedRootCount = InOther.FailedRootCount;
		RetainedInputBytes = InOther.RetainedInputBytes;
		RetainedWorkingBytes = InOther.RetainedWorkingBytes;
		RetainedLeaseCount = InOther.RetainedLeaseCount;
		InOther.ReservedPages = InOther.FailedRootCount = InOther.RetainedLeaseCount = 0;
		InOther.RetainedInputBytes = InOther.RetainedWorkingBytes = 0;
	}
	return *this;
}

bool FVoxelTerrainStageCache::IsRetryReady() const
{
	uint64 Version = 0;
	return bCapacityBlocked && Resources && Resources->TryGetReleaseVersionExcludingOwner(Consumer, Version) && Version > RetryCapacityVersion;
}

uint64 FVoxelTerrainStageCache::GetOutputVersion() const
{
	return Root ? Root->OutputVersion : 0;
}

bool FVoxelTerrainStageCache::HasPendingWork() const
{
	return Work && !Work->bTerminal;
}

bool FVoxelTerrainStageCache::AccumulateRetainedInput(const FVoxelTerrainResources& InResources,
	const FVoxelTerrainStageCache* InOther, TConstArrayView<uint64> InExcludedSerials,
	uint64& InOutBytes, int32& InOutLeaseCount, const bool bInIncludeCoverage) const
{
	if (!InOther) InOther = CapacityPrevious;
	const FVoxelTerrainStageCacheIndex* Indexes[] = {Root.Get(), Work ? Work->Candidate.Get() : nullptr,
		InOther ? InOther->Root.Get() : nullptr, InOther && InOther->Work ? InOther->Work->Candidate.Get() : nullptr};
	uint64 Serials[256] = {}; int32 SerialCount = 0;
	if (InExcludedSerials.Num() > UE_ARRAY_COUNT(Serials)) return false;
	for (const auto Serial : InExcludedSerials) Serials[SerialCount++] = Serial;
	const auto HasSerial = [&](const uint64 Serial)
	{
		for (const auto Excluded : InExcludedSerials) if (Serial == Excluded) return true;
		for (int32 I = 0; I < SerialCount; ++I) if (Serial == Serials[I]) return true;
		return false;
	};
	const auto AddLease = [&](const auto& Lease)
	{
		if (!Lease || !Lease->IsOwnedBy(InResources) || HasSerial(Lease->Serial())) return true;
		const uint64 Bytes = Lease->GetPool() == EVoxelTerrainResourcePool::Input ? Lease->Bytes() : 0;
		if (Bytes > MAX_uint64 - InOutBytes || InOutLeaseCount == MAX_int32 || SerialCount == UE_ARRAY_COUNT(Serials)) return false;
		InOutBytes += Bytes; ++InOutLeaseCount; Serials[SerialCount++] = Lease->Serial(); return true;
	};
	const FVoxelTerrainStageCache* Caches[] = {this, InOther};
	for (const auto* Cache : Caches)
	{
		if (!Cache) continue;
		if (!AddLease(Cache->SourceInputs ? Cache->SourceInputs->Lease : nullptr) || !AddLease(Cache->TerminalSources ? Cache->TerminalSources->Lease : nullptr) ||
			!AddLease(Cache->Work ? Cache->Work->Lease : nullptr)) return false;
	}
	const FVoxelCoverageIndex* Coverage[6] = {nullptr, nullptr, nullptr, nullptr,
		bTerminalAttempt ? &TerminalPredecessor : nullptr, InOther && InOther->bTerminalAttempt ? &InOther->TerminalPredecessor : nullptr};
	for (int32 I = 0; I < UE_ARRAY_COUNT(Indexes); ++I)
	{
		if (!Indexes[I]) continue;
		if (!AddLease(Indexes[I]->Lease) || !AddLease(Indexes[I]->Sources ? Indexes[I]->Sources->Lease : nullptr)) return false;
		Coverage[I] = &Indexes[I]->Predecessor;
	}
	const auto AddAggregate = [&](const auto* Node)
	{
		if (!Node || Node->AggregateLedger != &InResources) return false;
		for (const auto Serial : InExcludedSerials) if (Serial >= Node->MinimumSerial && Serial <= Node->MaximumSerial) return false;
		for (int32 I = 0; I < SerialCount; ++I) if (Serials[I] >= Node->MinimumSerial && Serials[I] <= Node->MaximumSerial) return false;
		return true;
	};
	const auto SumAggregate = [&](const auto* Node)
	{
		if (Node->InputBytes > MAX_uint64 - InOutBytes || Node->LeaseCount > MAX_int32 - InOutLeaseCount) return false;
		InOutBytes += Node->InputBytes; InOutLeaseCount += Node->LeaseCount; return true;
	};
	const auto VisitCache = [&](auto&& Self, const FVoxelTerrainStageCacheNode* const* Nodes) -> bool
	{
		const FVoxelTerrainStageCacheNode* Shared = nullptr; bool Different = false;
		for (int32 I = 0; I < 9; ++I) if (Nodes[I]) { if (!Shared) Shared = Nodes[I]; else Different |= Shared != Nodes[I]; }
		if (!Shared) return true;
		if (!Different && AddAggregate(Shared)) return SumAggregate(Shared);
		const int32 Before = SerialCount;
		for (int32 I = 0; I < 9; ++I) if (Nodes[I] && !AddLease(Nodes[I]->Lease)) return false;
		for (int32 Child = 0; Child < 8; ++Child)
		{
			const FVoxelTerrainStageCacheNode* Children[9] = {};
			for (int32 I = 0; I < 9; ++I) Children[I] = Nodes[I] ? Nodes[I]->Children[Child].Get() : nullptr;
			if (!Self(Self, Children)) return false;
		}
		SerialCount = Before; return true;
	};
	const FStageNativeCapacityScope* Active = nullptr;
	for (auto* Scope = StageNativeCapacityScope; Scope; Scope = Scope->Previous)
	{
		if (Scope->Cache != this && Scope->Cache != InOther) continue;
		if (Scope->Node) RefreshStageCacheAggregate(*Scope->Node);
		Active = Scope;
	}
	const auto VisitSpatial = [&](auto&& Self, const FVoxelTerrainStageSpatialNode* const* Nodes,
		const uint8 Level, const bool HasActive) -> bool
	{
		const FVoxelTerrainStageSpatialNode* Shared = nullptr; bool Different = false;
		for (int32 I = 0; I < 4; ++I) if (Nodes[I]) { if (!Shared) Shared = Nodes[I]; else Different |= Shared != Nodes[I]; }
		if (!Shared && !HasActive) return true;
		if (!HasActive && !Different && AddAggregate(Shared)) return SumAggregate(Shared);
		const int32 Before = SerialCount;
		const FVoxelTerrainStageCacheNode* Values[9] = {};
		for (int32 I = 0; I < 4; ++I)
		{
			if (Nodes[I] && !AddLease(Nodes[I]->Lease)) return false;
			Values[I * 2] = Nodes[I] ? Nodes[I]->Value.Get() : nullptr;
			Values[I * 2 + 1] = Nodes[I] ? Nodes[I]->FailureInputs.Get() : nullptr;
		}
		if (HasActive && Active && Active->Key.Level == Level) Values[8] = Active->RetainedNode;
		if (!VisitCache(VisitCache, Values)) return false;
		if (Level > 0) for (int32 Child = 0; Child < 8; ++Child)
		{
			const FVoxelTerrainStageSpatialNode* Children[4] = {};
			for (int32 I = 0; I < 4; ++I) Children[I] = Nodes[I] ? Nodes[I]->Children[Child].Get() : nullptr;
			const bool ChildActive = HasActive && Active && Active->Key.Level < Level && StageSpatialChildSlot(Active->Key, Level) == Child;
			if (!Self(Self, Children, uint8(Level - 1), ChildActive)) return false;
		}
		SerialCount = Before; return true;
	};
	for (int32 Slot = 0; Slot < 12; ++Slot)
	{
		const FVoxelTerrainStageSpatialNode* Nodes[4] = {};
		for (int32 I = 0; I < 4; ++I) Nodes[I] = Indexes[I] ? Indexes[I]->Slots[Slot].Get() : nullptr;
		if (!VisitSpatial(VisitSpatial, Nodes, 27, Active && StageSpatialRootSlot(Active->Key) == Slot)) return false;
	}
	return !bInIncludeCoverage || FVoxelCoverageIndex::AccumulateRetainedInputs(InResources, MakeArrayView(Coverage),
		MakeArrayView(Serials, SerialCount), InOutBytes, InOutLeaseCount);
}

void FVoxelTerrainStageCache::VisitRetainedCoverage(TFunctionRef<void(const FVoxelCoverageIndex&)> InVisitor) const
{
	if (bTerminalAttempt) InVisitor(TerminalPredecessor);
	if (Root) InVisitor(Root->Predecessor);
	if (Work && Work->Candidate) InVisitor(Work->Candidate->Predecessor);
}

namespace
{
int32 SubtractedRegionCount(const FVoxelCoverageRegion& Region, const FVoxelCoverageRegion& Cut)
{
	FVoxelCoverageRegion Intersection;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		Intersection.Min[Axis] = FMath::Max(Region.Min[Axis], Cut.Min[Axis]);
		Intersection.Max[Axis] = FMath::Min(Region.Max[Axis], Cut.Max[Axis]);
	}
	if (!Intersection.IsValid()) return 1;
	int32 Count = 0;
	for (int32 Axis = 0; Axis < 3; ++Axis)
		Count += int32(Region.Min[Axis] < Intersection.Min[Axis]) + int32(Region.Max[Axis] > Intersection.Max[Axis]);
	return Count;
}

uint8 ResolveWorkBoundedMaximumLevel(const int32 InSampleStepCells, const int32 InMaximumSamplesPerAxis, const uint8 InMaximumLevel)
{
	check(InSampleStepCells > 0);
	check(InMaximumSamplesPerAxis > 0);

	uint8 Result = 0;
	while (Result < InMaximumLevel)
	{
		const int64 NextSide = int64(16) << (Result + 1);
		if (NextSide / InSampleStepCells > InMaximumSamplesPerAxis)
		{
			break;
		}
		++Result;
	}
	return Result;
}

bool ResolveLocalPredecessorPreference(
	const FVoxelTerrainSpatialKey& InKey, const int32 InWorldMinZ, const int32 InWorldMaxZ,
	const FVoxelCoverageIndex* InPredecessorCoverage, uint8& OutPreference, FString& OutError)
{
	OutPreference = 0;
	if (!InPredecessorCoverage)
	{
		return true;
	}

	FIntVector Min;
	FIntVector Max;
	if (!InKey.TryBounds(InWorldMinZ, InWorldMaxZ, Min, Max))
	{
		return false;
	}

	FIntVector QueryMin, QueryMax;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		QueryMin[Axis] = int32(FMath::Max<int64>(MIN_int32, int64(Min[Axis]) - 1));
		QueryMax[Axis] = int32(FMath::Min<int64>(MAX_int32, int64(Max[Axis]) + 1));
	}
	QueryMin.Z = FMath::Max(QueryMin.Z, InWorldMinZ);
	QueryMax.Z = FMath::Min(QueryMax.Z, InWorldMaxZ);

	uint8 Preferred = 0;
	InPredecessorCoverage->AnyRecord(QueryMin, QueryMax, [&](const FVoxelCoverageRecord& Record)
	{
		const FIntVector& OtherMin = Record.Min;
		const FIntVector& OtherMax = Record.Max;

		const bool bOverlapX = OtherMax.X > Min.X && OtherMin.X < Max.X;
		const bool bOverlapY = OtherMax.Y > Min.Y && OtherMin.Y < Max.Y;
		const bool bOverlapZ = InKey.IsColumn() || (OtherMax.Z > Min.Z && OtherMin.Z < Max.Z);

		const bool bTouchesX = (OtherMax.X == Min.X || OtherMin.X == Max.X) && bOverlapY && bOverlapZ;
		const bool bTouchesY = (OtherMax.Y == Min.Y || OtherMin.Y == Max.Y) && bOverlapX && bOverlapZ;
		const bool bTouchesZ = !InKey.IsColumn() && (OtherMax.Z == Min.Z || OtherMin.Z == Max.Z) && bOverlapX && bOverlapY;

		if (bTouchesX || bTouchesY || bTouchesZ)
		{
			Preferred = FMath::Max(Preferred, Record.Geometry.Key.Level);
		}
		return false;
	});

	OutPreference = Preferred;
	return true;
}

bool ValidateStagePolicy(const EVoxelTerrainRepresentation Representation, const uint8 MaximumLevel,
	const int32 MaximumSamples, const int32 MinZ, const int32 MaxZ, const double Pixels,
	const FVoxelTerrainPageCostLimits& Limits, FString& Error)
{
	if ((Representation != EVoxelTerrainRepresentation::VoxelProxy && Representation != EVoxelTerrainRepresentation::Surface &&
		Representation != EVoxelTerrainRepresentation::Macro) || MaximumLevel > VoxelStreamingGeometry::MaximumSpatialLevel ||
		MinZ >= MaxZ || !FMath::IsFinite(Pixels) || Pixels <= 0.0 || !FMath::IsPowerOfTwo(MaximumSamples) ||
		MaximumSamples < (Representation == EVoxelTerrainRepresentation::VoxelProxy ? 32 : 16) ||
		MaximumSamples > (Representation == EVoxelTerrainRepresentation::VoxelProxy ? 128 : 256) ||
		(Representation == EVoxelTerrainRepresentation::VoxelProxy && MaximumLevel < 1) || Limits.Triangles == 0 || Limits.MeshBytes == 0 ||
		!FMath::IsFinite(Limits.BuildMilliseconds) || Limits.BuildMilliseconds <= 0.0)
	{
		Error = TEXT("Invalid coarse stage policy");
		return false;
	}
	return true;
}

bool ValidateStageSources(TConstArrayView<FVoxelTerrainStageSource> Sources, const bool bColumn, const uint8 MaximumLevel,
	const int32 MinZ, const int32 MaxZ, FString& Error)
{
	constexpr int64 MaximumRoots = 1 << 20;
	int64 TotalRoots = 0;
	for (const auto& Source : Sources)
	{
		if (Source.OuterRadiusCells < 0 || Source.VerticalHalfExtentCells < 0 || Source.MinimumSamplingDistanceCells < 0 ||
			Source.MinimumSamplingDistanceCells > Source.OuterRadiusCells || !FMath::IsFinite(Source.Source.VerticalFovDegrees) ||
			Source.Source.VerticalFovDegrees <= 0.0 || Source.Source.VerticalFovDegrees >= 180.0 || Source.Source.ViewportHeightPixels <= 0)
		{
			Error = TEXT("Invalid coarse stage source");
			return false;
		}
		FVoxelTerrainStageRootRange Range;
		if (!StageSourceRootRange(Source, bColumn, MaximumLevel, MinZ, MaxZ, Range)) continue;
		int64 Count = 1;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const int64 Width = int64(Range.Max[Axis]) - Range.Min[Axis] + 1;
			if (Width <= 0 || Width > MaximumRoots || Count > MaximumRoots / Width)
			{
				Error = TEXT("CapacityUnachievable: coarse root enumeration");
				return false;
			}
			Count *= Width;
		}
		if (TotalRoots > MaximumRoots - Count)
		{
			Error = TEXT("CapacityUnachievable: coarse root enumeration");
			return false;
		}
		TotalRoots += Count;
	}
	return true;
}

}

bool FVoxelTerrainStagePlanner::Build(const EVoxelTerrainRepresentation InRepresentation, TConstArrayView<FVoxelTerrainStageSource> InSources,
	const uint8 InMaximumLevel, const int32 InMaximumSamplesPerAxis, const int32 InWorldMinZ, const int32 InWorldMaxZ, const double InTargetPixels,
	const uint64 InGeneratorIdentity, const uint64 InMaterialRevision, FVoxelTerrainStagePlan& OutPlan, FString& OutError,
	const FVoxelCoverageIndex* InPredecessorCoverage, const FVoxelTerrainPageCostLimits& InCostLimits, FVoxelTerrainStageCache* InCache)
{
	if (!ValidateStagePolicy(InRepresentation, InMaximumLevel, InMaximumSamplesPerAxis, InWorldMinZ, InWorldMaxZ,
		InTargetPixels, InCostLimits, OutError) ||
		!ValidateStageSources(InSources, InRepresentation != EVoxelTerrainRepresentation::VoxelProxy,
			InMaximumLevel, InWorldMinZ, InWorldMaxZ, OutError))
	{
		return false;
	}
	FVoxelTerrainStageCache LocalCache;
	FVoxelTerrainStageCache& Cache = InCache ? *InCache : LocalCache;
	bool bCompletedBuild = false;
	const uint64 BeforeBuild = Cache.Resources ? Cache.Resources->ReleaseVersionExcludingOwner(Cache.Consumer) : 0;
	ON_SCOPE_EXIT
	{
		if (!bCompletedBuild)
		{
			Cache.bCapacityBlocked = OutError.StartsWith(TEXT("CapacityBlocked"));
			Cache.RetryCapacityVersion = BeforeBuild;
		}
	};
	bool bSameSources = Cache.SourceInputs && Cache.SourceInputs->Values.Num() == InSources.Num();
	for (int32 Index = 0; bSameSources && Index < InSources.Num(); ++Index)
	{
		bSameSources = SameStageInfluence(Cache.SourceInputs->Values[Index], InSources[Index]);
	}
	if (!bSameSources)
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (Cache.Resources)
		{
			uint64 Input = OutPlan.RetainedInputBytes;
			int32 Count = OutPlan.RetainedLeaseCount;
			if (!Cache.AccumulateRetainedInput(*Cache.Resources, nullptr, {}, Input, Count))
			{
				OutError = TEXT("CapacityUnachievable: coarse frozen roots");
				return false;
			}
			if (OutPlan.ResourceLease && OutPlan.ResourceLease->IsOwnedBy(*Cache.Resources))
			{
				Input += OutPlan.ResourceLease->GetPool() == EVoxelTerrainResourcePool::Input ? OutPlan.ResourceLease->Bytes() : 0;
				++Count;
			}
			const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainDomainSourceBatch>() +
				VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelTerrainStageSource, FDefaultAllocator>(InSources.Num());
			const auto Admission = Cache.Resources->TryAcquire(Cache.Consumer, EVoxelTerrainResourcePool::Input, Bytes, Lease, 0, Input, Count);
			if (Admission != EVoxelTerrainResourceAdmission::Granted)
			{
				OutError = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ?
					TEXT("CapacityBlocked: coarse frozen source") : TEXT("CapacityUnachievable: coarse frozen source");
				return false;
			}
		}
		auto Batch = Lease ? VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainDomainSourceBatch>(Lease) :
			MakeShared<FVoxelTerrainDomainSourceBatch, ESPMode::ThreadSafe>();
		Batch->Lease = MoveTemp(Lease);
		Batch->Values.Reserve(InSources.Num());
		Batch->Values.Append(InSources);
		Cache.SourceInputs = MoveTemp(Batch);
	}
	// 同步入口耗尽同一个增量游标；生产 Domain 仍按原预算分帧推进。
	for (;;)
	{
		if (AdvanceDirtyRoots(InRepresentation, InMaximumLevel, InMaximumSamplesPerAxis, InWorldMinZ, InWorldMaxZ,
			InTargetPixels, InGeneratorIdentity, InMaterialRevision, OutPlan, OutError, InPredecessorCoverage, InCostLimits, Cache))
		{
			bCompletedBuild = true;
			return true;
		}
		if (!OutError.StartsWith(TEXT("PlanningPending")))
		{
			return false;
		}
	}
}

bool FVoxelTerrainStagePlanner::AdvanceDirtyRoots(const EVoxelTerrainRepresentation Representation, const uint8 MaximumLevel,
	const int32 MaximumSamples, const int32 MinZ, const int32 MaxZ, const double Pixels, const uint64 Generator,
	const uint64 Material, FVoxelTerrainStagePlan& OutPlan, FString& Error, const FVoxelCoverageIndex* Predecessor,
	const FVoxelTerrainPageCostLimits& Limits, FVoxelTerrainStageCache& Cache, const int32 RootBudget, const bool bExportUnchangedOutput)
{
	if (!Cache.SourceInputs || RootBudget <= 0)
	{
		Error = TEXT("Invalid coarse planning input or root budget");
		return false;
	}
	if (!ValidateStagePolicy(Representation, MaximumLevel, MaximumSamples, MinZ, MaxZ, Pixels, Limits, Error) ||
		!ValidateStageSources(Cache.SourceInputs->Values, Representation != EVoxelTerrainRepresentation::VoxelProxy,
			MaximumLevel, MinZ, MaxZ, Error))
	{
		return false;
	}
	const FVoxelTerrainStageCache Previous = Cache;
	Cache.CapacityPrevious = &Previous;
	ON_SCOPE_EXIT { Cache.CapacityPrevious = nullptr; };
	const uint64 Before = Cache.Resources ? Cache.Resources->ReleaseVersionExcludingOwner(Cache.Consumer) : 0;
	bool Completed = false;
	ON_SCOPE_EXIT
	{
		if (!Completed)
		{
			Cache.bCapacityBlocked = Error.StartsWith(TEXT("CapacityBlocked")); Cache.RetryCapacityVersion = Before;
			if (Error.StartsWith(TEXT("CapacityUnachievable")))
			{
				Cache.bTerminalAttempt = true; Cache.TerminalSources = Cache.SourceInputs;
				Cache.TerminalPredecessor = Predecessor ? *Predecessor : FVoxelCoverageIndex();
				Cache.TerminalLevel = MaximumLevel; Cache.TerminalSamples = MaximumSamples;
				Cache.TerminalMinZ = MinZ; Cache.TerminalMaxZ = MaxZ; Cache.TerminalPixels = Pixels;
				Cache.TerminalGenerator = Generator; Cache.TerminalMaterial = Material; Cache.TerminalLimits = Limits;
			}
			if (Cache.Work && !Cache.Work->bInitialized) Cache.Work.Reset();
			if (Cache.Work && !Cache.bCapacityBlocked && !Error.StartsWith(TEXT("PlanningPending"))) Cache.Work->bTerminal = true;
		}
	};
	const bool Column = Representation != EVoxelTerrainRepresentation::VoxelProxy;
	FVoxelCoverageIndex EmptyCoverage;
	const FVoxelCoverageIndex& InputCoverage = Predecessor ? *Predecessor : EmptyCoverage;
	const auto SameCoverage = [&](const FVoxelCoverageIndex& Old)
	{
		bool Changed = false;
		InputCoverage.VisitChangedRecords(Old, [&](const auto*, const auto*) { Changed = true; });
		return !Changed;
	};
	if (Cache.bTerminalAttempt)
	{
		if (Cache.TerminalSources == Cache.SourceInputs && Cache.TerminalLevel == MaximumLevel && Cache.TerminalSamples == MaximumSamples &&
			Cache.TerminalMinZ == MinZ && Cache.TerminalMaxZ == MaxZ && Cache.TerminalPixels == Pixels &&
			Cache.TerminalGenerator == Generator && Cache.TerminalMaterial == Material && Cache.TerminalLimits.Triangles == Limits.Triangles &&
			Cache.TerminalLimits.MeshBytes == Limits.MeshBytes && Cache.TerminalLimits.BuildMilliseconds == Limits.BuildMilliseconds && SameCoverage(Cache.TerminalPredecessor))
		{ Error = TEXT("CapacityUnachievable: original coarse input attempt terminated"); return false; }
		Cache.bTerminalAttempt = false; Cache.TerminalSources.Reset(); Cache.TerminalPredecessor.Reset();
	}
	const auto SameInputs = [&](const FVoxelTerrainStageCacheIndex* Index)
	{
		return Index && Index->WorldEpoch == Cache.Consumer.WorldEpoch && Index->Sources == Cache.SourceInputs &&
			Index->SamePolicy(MaximumLevel, MaximumSamples, MinZ, MaxZ, Pixels, Generator, Material, Limits) && SameCoverage(Index->Predecessor);
	};
	const bool RetryFailedRoots = SameInputs(Cache.Root.Get()) && Cache.bCapacityBlocked && Cache.IsRetryReady();
	FVoxelTerrainStagePlan Plan;
	Plan.Resources = Cache.Resources; Plan.Consumer = Cache.Consumer; Plan.CapacityCache = &Cache;
	Plan.RetainedInputBytes = OutPlan.RetainedInputBytes; Plan.RetainedWorkingBytes = OutPlan.RetainedWorkingBytes;
	Plan.RetainedLeaseCount = OutPlan.RetainedLeaseCount;
	if (OutPlan.ResourceLease && Cache.Resources && OutPlan.ResourceLease->IsOwnedBy(*Cache.Resources))
	{
		Plan.RetainedInputBytes += OutPlan.ResourceLease->GetPool() == EVoxelTerrainResourcePool::Input ? OutPlan.ResourceLease->Bytes() : 0;
		Plan.RetainedWorkingBytes += OutPlan.ResourceLease->GetPool() == EVoxelTerrainResourcePool::Working ? OutPlan.ResourceLease->Bytes() : 0;
		++Plan.RetainedLeaseCount;
	}
	if (Cache.Work && SameInputs(Cache.Work->Candidate.Get()) && Cache.Work->bInitialized)
	{
		if (Cache.Work->bTerminal) { Error = TEXT("CapacityUnachievable: original coarse root attempt terminated"); return false; }
		if (Cache.bCapacityBlocked && !Cache.IsRetryReady()) { Error = TEXT("CapacityBlocked: coarse root awaiting external release"); return false; }
	}
	else if (!SameInputs(Cache.Root.Get()) || RetryFailedRoots)
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> WorkLease, IndexLease;
		if (Cache.Resources)
		{
			uint64 Input = 0; int32 Count = 0;
			if (!StagePlanRetainedFloor(Plan, Input, Count)) { Error = TEXT("CapacityUnachievable: coarse work original root floor"); return false; }
			const auto Admission = Cache.Resources->TryAcquire(Cache.Consumer, EVoxelTerrainResourcePool::Input,
				VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainStageWork>(), WorkLease, 0, Input, Count);
			if (Admission != EVoxelTerrainResourceAdmission::Granted)
			{ Error = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: coarse work root") : TEXT("CapacityUnachievable: coarse work root"); return false; }
			Input += WorkLease->Bytes(); ++Count;
			const auto IndexAdmission = Cache.Resources->TryAcquire(Cache.Consumer, EVoxelTerrainResourcePool::Input,
				VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainStageCacheIndex>(), IndexLease, 0, Input, Count);
			if (IndexAdmission != EVoxelTerrainResourceAdmission::Granted)
			{ Error = IndexAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: coarse candidate root") : TEXT("CapacityUnachievable: coarse candidate root"); return false; }
		}
		auto Work = WorkLease ? VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainStageWork>(WorkLease) : MakeShared<FVoxelTerrainStageWork, ESPMode::ThreadSafe>();
		Work->Lease = MoveTemp(WorkLease); Work->bRetryOnly = RetryFailedRoots; Work->bOutputChanged = !RetryFailedRoots;
		Work->Candidate = IndexLease ? VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainStageCacheIndex>(IndexLease) : MakeShared<FVoxelTerrainStageCacheIndex, ESPMode::ThreadSafe>();
		const auto* Base = Previous.Work ? Previous.Work->Candidate.Get() : Previous.Root.Get();
		const bool Compatible = Base && Base->WorldEpoch == Cache.Consumer.WorldEpoch && Base->MaximumLevel == MaximumLevel;
		if (Compatible) for (int32 Slot = 0; Slot < 12; ++Slot) Work->Candidate->Slots[Slot] = Base->Slots[Slot];
		Work->Candidate->Lease = MoveTemp(IndexLease); Work->Candidate->WorldEpoch = Cache.Consumer.WorldEpoch;
		Work->Candidate->Sources = Cache.SourceInputs; Work->Candidate->Predecessor = InputCoverage;
		Work->Candidate->MaximumLevel = MaximumLevel; Work->Candidate->MaximumSamples = MaximumSamples;
		Work->Candidate->MinZ = MinZ; Work->Candidate->MaxZ = MaxZ; Work->Candidate->TargetPixels = Pixels;
		Work->Candidate->Generator = Generator; Work->Candidate->Material = Material; Work->Candidate->Limits = Limits;
		const bool PolicyChanged = !Previous.Root || Previous.Root->WorldEpoch != Cache.Consumer.WorldEpoch ||
			!Previous.Root->SamePolicy(MaximumLevel, MaximumSamples, MinZ, MaxZ, Pixels, Generator, Material, Limits);
		const FVoxelTerrainDomainSourceBatch* Batches[] = {Previous.Root ? Previous.Root->Sources.Get() : nullptr,
			Previous.Work ? Previous.Work->Candidate->Sources.Get() : nullptr, Cache.SourceInputs.Get()};
		int64 RangeCapacity = int64(Cache.SourceInputs->Values.Num());
		for (int32 I = 0; I < 2; ++I) if (Batches[I]) RangeCapacity += Batches[I]->Values.Num();
		int32 ChangedRecords = 0;
		InputCoverage.VisitChangedRecords(Previous.Root ? Previous.Root->Predecessor : EmptyCoverage,
			[&](const auto* Now, const auto* Old) { ChangedRecords += int32(Now != nullptr) + int32(Old != nullptr); });
		RangeCapacity += ChangedRecords;
		if (Previous.Work) InputCoverage.VisitChangedRecords(Previous.Work->Candidate->Predecessor,
			[&](const auto* Now, const auto* Old) { RangeCapacity += int32(Now != nullptr) + int32(Old != nullptr); });
		if (RangeCapacity > (1 << 20)) { Error = TEXT("CapacityUnachievable: coarse source delta ranges"); return false; }
		// 先发布操作引用以纳入下一次 grant 的真实强持聚合；accepted Root 仍保持旧值。
		Cache.Work = Work; Cache.bCapacityBlocked = false; Cache.RebuiltNodes = Cache.ReusedNodes = Cache.FailedRoots = 0;
		int32 RetryCount = 0;
		if (RetryFailedRoots) for (const auto& Slot : Previous.Root->Slots) if (Slot) RetryCount += Slot->RetryRootCount;
		if (!ReserveStageWork(*Work, RetryFailedRoots ? 0 : int32(RangeCapacity), RetryCount, RetryCount, Plan, Error)) return false;
		const auto FindSource = [](const FVoxelTerrainDomainSourceBatch* Batch, const FGuid& Id) -> const FVoxelTerrainStageSource*
		{
			if (Batch) for (const auto& Source : Batch->Values) if (Source.Source.Id == Id) return &Source;
			return nullptr;
		};
		if (!RetryFailedRoots) for (int32 I = 0; I < 3; ++I)
		{
			if (!Batches[I]) continue;
			bool Duplicate = false; for (int32 J = 0; J < I; ++J) Duplicate |= Batches[I] == Batches[J];
			if (Duplicate) continue;
			for (const auto& Source : Batches[I]->Values)
			{
				if (Source.OuterRadiusCells < 0 || Source.VerticalHalfExtentCells < 0 || Source.MinimumSamplingDistanceCells < 0 || Source.MinimumSamplingDistanceCells > Source.OuterRadiusCells || !FMath::IsFinite(Source.Source.VerticalFovDegrees) ||
					Source.Source.VerticalFovDegrees <= 0.0 || Source.Source.VerticalFovDegrees >= 180.0 || Source.Source.ViewportHeightPixels <= 0)
				{ Error = TEXT("Invalid incremental coarse source"); return false; }
				const auto* Old = FindSource(Batches[0], Source.Source.Id), *Pending = FindSource(Batches[1], Source.Source.Id), *Now = FindSource(Batches[2], Source.Source.Id);
				const bool SameAccepted = Old && Now && SameStageInfluence(*Old, *Now);
				const bool SamePending = !Batches[1] || (Pending && Now && SameStageInfluence(*Pending, *Now));
				if (!PolicyChanged && SameAccepted && SamePending) continue;
				FVoxelTerrainStageRootRange Range;
				if (StageSourceRootRange(Source, Column, MaximumLevel, MinZ, MaxZ, Range)) Work->Ranges.Add(Range);
			}
		}
		const auto AddPredecessorChanges = [&](const FVoxelCoverageIndex& Old)
		{
			InputCoverage.VisitChangedRecords(Old, [&](const auto* Now, const auto* BeforeRecord)
			{
				for (const auto* Record : {Now, BeforeRecord})
				{
					if (!Record) continue;
					FIntVector Min = Record->Min, Max = Record->Max;
					for (int32 Axis = 0; Axis < 3; ++Axis)
					{ Min[Axis] = int32(FMath::Max<int64>(MIN_int32, int64(Min[Axis]) - 1)); Max[Axis] = int32(FMath::Min<int64>(MAX_int32, int64(Max[Axis]) + 1)); }
					FVoxelTerrainStageRootRange Range;
					if (StageRangeForBounds(Min, Max, Column, MaximumLevel, Range)) Work->Ranges.Add(Range);
				}
			});
		};
		if (!RetryFailedRoots)
		{
			AddPredecessorChanges(Previous.Root ? Previous.Root->Predecessor : EmptyCoverage);
			if (Previous.Work) AddPredecessorChanges(Previous.Work->Candidate->Predecessor);
		}
		else
		{
			VisitStageFailedRoots(*Previous.Root, [&](const FVoxelTerrainStageSpatialNode& Node)
			{
				if (Node.FailureAdmission != EVoxelTerrainResourceAdmission::CapacityBlocked) return;
				uint64 Version = 0;
				if (!Cache.Resources || !Cache.Resources->TryGetReleaseVersionExcludingOwner(Node.FailureConsumer, Version) || Version <= Node.WaitVersion) return;
				Work->Membership.Add(Node.Key, Node.FailedMaximumPageLevel); Work->Queue.Add({Node.Key, Node.FailedMaximumPageLevel});
			});
		}
		Work->bInitialized = true;
	}
	else if (Cache.Work) Cache.Work.Reset();
	if (Cache.Work)
	{
		auto& Work = *Cache.Work;
		int32 Processed = 0, Enumerated = 0;
		while (Processed < RootBudget && Enumerated < RootBudget * 4)
		{
			if (Work.QueueCursor >= Work.Queue.Num())
			{
				if (!Work.Ranges.IsValidIndex(Work.RangeCursor)) break;
				const int32 BeforeCursor = Work.RangeCursor;
				const auto BeforeRange = Work.Ranges[BeforeCursor];
				FVoxelTerrainSpatialKey Key;
				if (!NextStageRangeRoot(Work, Representation, MaximumLevel, Key)) break;
				++Enumerated;
				if (!EnqueueStageRoot(Work, Key, MAX_uint8, Plan, Error))
				{ Work.RangeCursor = BeforeCursor; Work.Ranges[BeforeCursor] = BeforeRange; return false; }
				if (Work.QueueCursor >= Work.Queue.Num()) continue;
			}
			const auto Request = Work.Queue[Work.QueueCursor];
			if (Work.Membership.FindRef(Request.Key) < Request.MaximumPageLevel) { ++Work.QueueCursor; continue; }
			FStageCacheNodePtr Updated;
			const auto PreviousNode = FindStageSpatialRoot(*Work.Candidate, Request.Key);
			const auto* Entry = FindStageSpatialEntry(*Work.Candidate, Request.Key);
			if (Entry && Entry->FailureInputs && Entry->FailureAdmission != EVoxelTerrainResourceAdmission::None &&
				StageFailureInputsCurrent(*Entry->FailureInputs, Request.Key, Cache.SourceInputs->Values, Work.Candidate->Predecessor,
					MaximumLevel, MaximumSamples, MinZ, MaxZ, Pixels, Generator, Material, Limits, Request.MaximumPageLevel, Cache.Resources.Get()))
			{
				uint64 Version = 0;
				const bool Retry = Entry->FailureAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked && Cache.Resources &&
					Cache.Resources->TryGetReleaseVersionExcludingOwner(Entry->FailureConsumer, Version) && Version > FMath::Max(Entry->WaitVersion, Cache.RetryCapacityVersion);
				if (!Retry) { ++Work.QueueCursor; ++Processed; continue; }
			}

			FVoxelTerrainStagePlan RootPlan;
			RootPlan.Resources = Cache.Resources; RootPlan.Consumer = Cache.Consumer; RootPlan.CapacityCache = &Cache;
			RootPlan.RetainedInputBytes = Plan.RetainedInputBytes; RootPlan.RetainedWorkingBytes = Plan.RetainedWorkingBytes;
			RootPlan.RetainedLeaseCount = Plan.RetainedLeaseCount;
			int32 Visited = 0;
			const uint64 RootBefore = Cache.Resources ? Cache.Resources->ReleaseVersionExcludingOwner(Cache.Consumer) : 0;
			if (!Visit(Request.Key, Cache.SourceInputs->Values, MaximumLevel, MaximumSamples, MinZ, MaxZ, Pixels, Generator, Material,
				RootPlan, Visited, Error, &Work.Candidate->Predecessor, Limits, &Cache, PreviousNode, &Updated, Request.MaximumPageLevel))
			{
				++Cache.FailedRoots; ++Work.FailedRoots;
				// 没有已授额的旧输出／冻结失败输入时，不能以空根冒充成功。
				if (!PreviousNode || !Updated) return false;
				const auto Admission = Error.StartsWith(TEXT("CapacityBlocked")) ? EVoxelTerrainResourceAdmission::CapacityBlocked :
					Error.StartsWith(TEXT("CapacityUnachievable")) ? EVoxelTerrainResourceAdmission::CapacityUnachievable : EVoxelTerrainResourceAdmission::InvalidRequest;
				if (Work.bRetryOnly && Entry && Entry->FailureAdmission == Admission && Entry->FailureInputs &&
					StageFailureInputsCurrent(*Entry->FailureInputs, Request.Key, Cache.SourceInputs->Values, Work.Candidate->Predecessor,
						MaximumLevel, MaximumSamples, MinZ, MaxZ, Pixels, Generator, Material, Limits, Request.MaximumPageLevel, Cache.Resources.Get()))
				{ ++Work.QueueCursor; ++Processed; Error.Reset(); continue; }
				FStageNativeCapacityScope FailedInputScope(&Cache, Updated.Get(), Request.Key);
				if (!CommitStageSpatialRoot(Cache, Work, Request.Key, PreviousNode, RootPlan, Error, Updated, Admission, RootBefore, Request.MaximumPageLevel)) return false;
				Work.bOutputChanged = true;
				++Work.QueueCursor; ++Processed; Error.Reset(); continue;
			}
			FStageNativeCapacityScope RetainedRoot(&Cache, Updated.Get(), Request.Key);
			if (!CommitStageSpatialRoot(Cache, Work, Request.Key, Updated && Updated->PageCount ? Updated : nullptr, RootPlan, Error)) return false;
			Work.bOutputChanged = true;
			// 只检查本根变化页接触的邻接根；更细请求单调降低，后续推进沿原队列恢复。
			for (const auto& Pair : RootPlan.Pages)
			{
				FIntVector PageMin, PageMax;
				if (!Pair.Key.TryBounds(MinZ, MaxZ, PageMin, PageMax)) { Error = TEXT("Invalid changed root output bounds"); return false; }
				for (int32 Axis = 0; Axis < (Column ? 2 : 3); ++Axis) for (const int32 Sign : {-1, 1})
				{
					if (Sign < 0 && PageMin[Axis] == MIN_int32) continue;
					FIntVector Point = PageMin + (PageMax - PageMin) / 2; Point[Axis] = Sign < 0 ? PageMin[Axis] - 1 : PageMax[Axis];
					const int64 Side = int64(16) << MaximumLevel;
					const FVoxelTerrainSpatialKey NeighborRoot{Representation, FIntVector(int32(VoxelStreamingGeometry::FloorDivide(Point.X, Side)),
						int32(VoxelStreamingGeometry::FloorDivide(Point.Y, Side)), Column ? 0 : int32(VoxelStreamingGeometry::FloorDivide(Point.Z, Side))), MaximumLevel};
					const auto Neighbor = FindStageSpatialRoot(*Work.Candidate, NeighborRoot);
					bool BalanceReady = true;
					const auto Touch = [&](auto&& Self, const FVoxelTerrainStageCacheNode* Node) -> void
					{
						if (!Node || !BalanceReady) return;
						FIntVector OtherMin, OtherMax;
						if (!Node->Geometry.Key.TryBounds(MinZ, MaxZ, OtherMin, OtherMax)) return;
						const int32 Face = Sign < 0 ? PageMin[Axis] : PageMax[Axis];
						if (OtherMin[Axis] > Face || OtherMax[Axis] < Face) return;
						for (int32 A = 0; A < (Column ? 2 : 3); ++A) if (A != Axis && (OtherMin[A] >= PageMax[A] || OtherMax[A] <= PageMin[A])) return;
						if (!Node->bPage) { for (const auto& Child : Node->Children) Self(Self, Child.Get()); return; }
						if ((Sign < 0 ? OtherMax[Axis] : OtherMin[Axis]) != Face) return;
						if (Node->Geometry.Key.Level > Pair.Key.Level + 1)
							BalanceReady = EnqueueStageRoot(Work, NeighborRoot, uint8(Pair.Key.Level + 1), RootPlan, Error);
						else if (Pair.Key.Level > Node->Geometry.Key.Level + 1)
							BalanceReady = EnqueueStageRoot(Work, Request.Key, uint8(Node->Geometry.Key.Level + 1), RootPlan, Error);
					};
					Touch(Touch, Neighbor.Get()); if (!BalanceReady) return false;
				}
			}
			++Work.QueueCursor; ++Processed;
		}
		if (Work.QueueCursor < Work.Queue.Num() || Work.RangeCursor < Work.Ranges.Num())
		{ Cache.bCapacityBlocked = false; Error = TEXT("PlanningPending: bounded coarse dirty roots"); return false; }
	}
	const bool NoProgressRetry = Cache.Work && Cache.Work->bRetryOnly && !Cache.Work->bOutputChanged;
	if (NoProgressRetry) Cache.Work.Reset();
	const bool ChangedOutput = Cache.Work.IsValid();
	if (bExportUnchangedOutput || ChangedOutput)
	{
		const auto* Output = Cache.Work ? Cache.Work->Candidate.Get() : Cache.Root.Get();
		if (Output)
		{
			int64 Pages = 0; for (const auto& Slot : Output->Slots) if (Slot) { Pages += Slot->PageCount; Plan.FailedRootCount += Slot->FailedRootCount; }
			if (Pages > (1 << 20) || !ReserveStagePlan(Plan, int32(Pages), Error)) return false;
			const auto Replay = [&](auto&& Self, const FVoxelTerrainStageSpatialNode* Node) -> bool
			{
				if (!Node) return true;
				if (!ReplayStageCache(Node->Value, Plan, Error)) return false;
				for (const auto& Child : Node->Children) if (!Self(Self, Child.Get())) return false;
				return true;
			};
			for (const auto& Slot : Output->Slots) if (!Replay(Replay, Slot.Get())) return false;
		}
	}
	if (Cache.Work)
	{
		Cache.Work->Candidate->OutputVersion = NextStageCommittedOutputVersion.IncrementExchange() + 1;
		Cache.Root = Cache.Work->Candidate; Cache.Work.Reset();
		// 本 Stage immutable accepted root 接管后才标记；中途的 Candidate/path 不产生可唤醒退休事件。
		const auto MarkCache = [&](auto&& Self, const FVoxelTerrainStageCacheNode* Node) -> void
		{
			if (!Node || (Node->Lease && Node->Lease->IsPublished())) return;
			if (Node->Lease) Node->Lease->MarkPublished();
			for (const auto& Child : Node->Children) Self(Self, Child.Get());
		};
		const auto MarkSpatial = [&](auto&& Self, const FVoxelTerrainStageSpatialNode* Node) -> void
		{
			if (!Node || (Node->Lease && Node->Lease->IsPublished())) return;
			if (Node->Lease) Node->Lease->MarkPublished();
			MarkCache(MarkCache, Node->Value.Get());
			MarkCache(MarkCache, Node->FailureInputs.Get());
			for (const auto& Child : Node->Children) Self(Self, Child.Get());
		};
		if (Cache.Root->Lease) Cache.Root->Lease->MarkPublished();
		if (Cache.Root->Sources && Cache.Root->Sources->Lease) Cache.Root->Sources->Lease->MarkPublished();
		Cache.Root->Predecessor.MarkPublishedInputs();
		for (const auto& Slot : Cache.Root->Slots) MarkSpatial(MarkSpatial, Slot.Get());
	}
	Plan.CapacityCache = nullptr; Plan.RetainedInputBytes = Plan.RetainedWorkingBytes = 0; Plan.RetainedLeaseCount = 0;
	if (bExportUnchangedOutput || ChangedOutput) OutPlan = MoveTemp(Plan);
	Cache.bCapacityBlocked = false; Cache.FailedRoots = 0; Cache.RetryCapacityVersion = MAX_uint64;
	if (Cache.Root) VisitStageFailedRoots(*Cache.Root, [&](const FVoxelTerrainStageSpatialNode& Node)
	{
		++Cache.FailedRoots;
		if (Node.FailureAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked)
		{ Cache.bCapacityBlocked = true; Cache.RetryCapacityVersion = FMath::Min(Cache.RetryCapacityVersion, Node.WaitVersion); }
	});
	if (NoProgressRetry && Cache.bCapacityBlocked) Cache.RetryCapacityVersion = FMath::Max(Cache.RetryCapacityVersion, Before);
	if (!Cache.bCapacityBlocked) Cache.RetryCapacityVersion = 0;
	Error.Reset(); Completed = true;
	return true;
}

bool FVoxelTerrainStagePlanner::ExportCurrentPlan(FVoxelTerrainStageCache& Cache, FVoxelTerrainStagePlan& OutPlan,
	FString& Error, const bool bInputAlreadyAccounted)
{
	FVoxelTerrainStagePlan Plan;
	Plan.Resources = Cache.Resources; Plan.Consumer = Cache.Consumer;
	Plan.RetainedInputBytes = OutPlan.RetainedInputBytes; Plan.RetainedWorkingBytes = OutPlan.RetainedWorkingBytes;
	Plan.RetainedLeaseCount = OutPlan.RetainedLeaseCount; Plan.CapacityCache = bInputAlreadyAccounted ? nullptr : &Cache;
	int64 Count = 0;
	if (Cache.Root) for (const auto& Slot : Cache.Root->Slots) if (Slot) { Count += Slot->PageCount; Plan.FailedRootCount += Slot->FailedRootCount; }
	if (Count > (1 << 20) || !ReserveStagePlan(Plan, int32(Count), Error)) return false;
	const auto Visit = [&](auto&& Self, const FVoxelTerrainStageSpatialNode* Node) -> bool
	{
		if (!Node) return true;
		if (!ReplayStageCache(Node->Value, Plan, Error)) return false;
		for (const auto& Child : Node->Children) if (!Self(Self, Child.Get())) return false;
		return true;
	};
	if (Cache.Root) for (const auto& Slot : Cache.Root->Slots) if (!Visit(Visit, Slot.Get())) return false;
	Plan.CapacityCache = nullptr; Plan.RetainedInputBytes = Plan.RetainedWorkingBytes = 0; Plan.RetainedLeaseCount = 0;
	OutPlan = MoveTemp(Plan); Error.Reset(); return true;
}

bool FVoxelTerrainStagePlanner::Visit(const FVoxelTerrainSpatialKey& InKey, TConstArrayView<FVoxelTerrainStageSource> InSources, const uint8 InMaximumLevel,
	const int32 InMaximumSamplesPerAxis, const int32 InWorldMinZ, const int32 InWorldMaxZ, const double InTargetPixels, const uint64 InGeneratorIdentity,
	const uint64 InMaterialRevision, FVoxelTerrainStagePlan& InOutPlan, int32& InOutVisited, FString& OutError,
	const FVoxelCoverageIndex* InPredecessorCoverage, const FVoxelTerrainPageCostLimits& InCostLimits, FVoxelTerrainStageCache* InCache,
	FStageCacheNodePtr InPrevious, FStageCacheNodePtr* OutCacheNode, const uint8 InMaximumPageLevel)
{
	if (++InOutVisited > (1 << 20))
	{
		OutError = TEXT("Stage traversal exceeds the emergency node guard");
		return false;
	}

	FIntVector Min;
	FIntVector Max;
	if (!InKey.TryBounds(InWorldMinZ, InWorldMaxZ, Min, Max))
	{
		OutError = TEXT("Stage node overflows the supported stable coordinate grid");
		return false;
	}

	if (!InKey.IsColumn() && (Max.Z <= InWorldMinZ || Min.Z >= InWorldMaxZ))
	{
		return true;
	}

	FVoxelCoverageRegion Required{Min, Max};
	Required.Min.Z = FMath::Max(Required.Min.Z, InWorldMinZ);
	Required.Max.Z = FMath::Min(Required.Max.Z, InWorldMaxZ);
	TSharedPtr<FVoxelTerrainStageCacheNode, ESPMode::ThreadSafe> CacheNode;
	if (InCache && OutCacheNode)
	{
		if (!PrepareStageCacheNode(InKey, Min, Max, InSources, InPredecessorCoverage, *InCache, InOutPlan, CacheNode, OutError)) return false;
		CacheNode->MaximumPageLevel = InMaximumPageLevel; CacheNode->PolicyMaximumLevel = InMaximumLevel;
		CacheNode->MaximumSamples = InMaximumSamplesPerAxis; CacheNode->TargetPixels = InTargetPixels; CacheNode->PolicyLimits = InCostLimits;
		CacheNode->Geometry = {InKey, 1, InWorldMinZ, InWorldMaxZ, InGeneratorIdentity, InMaterialRevision};
		if (InPrevious && SameStageCacheInputs(*InPrevious, *CacheNode))
		{
			++InCache->ReusedNodes; *OutCacheNode = InPrevious;
			return ReplayStageCache(InPrevious, InOutPlan, OutError);
		}
		++InCache->RebuiltNodes; *OutCacheNode = CacheNode;
	}

	FStageNativeCapacityScope NativeScope(InCache, CacheNode.Get(), InKey);
	const bool bOwnCacheNode = (!InCache || !InCache->Work) && CacheNode && CacheNode->Lease && InOutPlan.Resources && CacheNode->Lease->IsOwnedBy(*InOutPlan.Resources);
	const uint64 CacheNodeInput = bOwnCacheNode ? CacheNode->Lease->Bytes() : 0;
	if (bOwnCacheNode) { InOutPlan.RetainedInputBytes += CacheNodeInput; ++InOutPlan.RetainedLeaseCount; }
	ON_SCOPE_EXIT
	{
		if (bOwnCacheNode) { InOutPlan.RetainedInputBytes -= CacheNodeInput; --InOutPlan.RetainedLeaseCount; }
	};
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ContributionLease;
	TArray<FVoxelCoverageRegion> Contributions;
	TArray<FVoxelCoverageRegion> Next;
	if (!ReserveStageScratch(Contributions, 1, 0, 65536, InOutPlan, ContributionLease, OutError)) return false;
	Contributions.Add(Required);

	if (InPredecessorCoverage)
	{
		bool bPartitionReady = true;
		InPredecessorCoverage->AnyRecord(Required.Min, Required.Max, [&](const FVoxelCoverageRecord& Record)
		{
			Next.Reset();
			int64 RequiredPieces = 0;
			for (const auto& Piece : Contributions) RequiredPieces += SubtractedRegionCount(Piece, {Record.Min, Record.Max});
			if (RequiredPieces > 65536)
			{ OutError = TEXT("CapacityUnachievable: coarse contribution partition"); bPartitionReady = false; return true; }
			if (!ReserveStageScratch(Next, int32(RequiredPieces), VoxelGenerationAllocation::AllocatedBytes(Contributions),
				65536, InOutPlan, ContributionLease, OutError)) { bPartitionReady = false; return true; }
			for (const auto& Piece : Contributions) FVoxelCoverageEvaluationTask::Subtract(Piece, {Record.Min, Record.Max}, Next);
			check(Next.Num() == RequiredPieces);
			Swap(Contributions, Next);
			return Contributions.IsEmpty();
		});
		if (!bPartitionReady) return false;
		if (Contributions.IsEmpty()) return true;
	}

	bool bRelevant = false;
	bool bResolutionFloor = false;
	int32 Sampling = static_cast<int32>(InKey.SideCells());
	int32 Priority = MAX_int32;

	for (const FVoxelTerrainStageSource& Entry : InSources)
	{
		if (Entry.OuterRadiusCells == 0)
		{
			continue;
		}

		const FVoxelStreamingSource& Source = Entry.Source;
		double Distance = TNumericLimits<double>::Max();

		for (const FVoxelCoverageRegion& Piece : Contributions)
		{
			const double NearX = FMath::Clamp(double(Source.Center.X), double(Piece.Min.X), double(Piece.Max.X));
			const double NearY = FMath::Clamp(double(Source.Center.Y), double(Piece.Min.Y), double(Piece.Max.Y));
			const double NearZ = FMath::Clamp(double(Source.Center.Z), double(Piece.Min.Z), double(Piece.Max.Z));

			const double Horizontal = FVector2D::Distance(FVector2D(Source.Center.X, Source.Center.Y), FVector2D(NearX, NearY));

			if (Horizontal > Entry.OuterRadiusCells ||
				(!InKey.IsColumn() && (Entry.VerticalHalfExtentCells == 0 || FMath::Abs(NearZ - Source.Center.Z) > Entry.VerticalHalfExtentCells)))
			{
				continue;
			}

			const double FarX = FMath::Max(FMath::Abs(double(Piece.Min.X) - Source.Center.X), FMath::Abs(double(Piece.Max.X) - Source.Center.X));
			const double FarY = FMath::Max(FMath::Abs(double(Piece.Min.Y) - Source.Center.Y), FMath::Abs(double(Piece.Max.Y) - Source.Center.Y));

			if (Entry.InnerRadiusCells > 0 && FarX * FarX + FarY * FarY <= FMath::Square(double(Entry.InnerRadiusCells)))
			{
				continue;
			}

			Distance = FMath::Min(Distance,
				FMath::Max(double(Entry.InnerRadiusCells), FVector(NearX - Source.Center.X, NearY - Source.Center.Y, NearZ - Source.Center.Z).Size()));
		}

		if (Distance == TNumericLimits<double>::Max())
		{
			continue;
		}

		bRelevant = true;
		Priority = FMath::Min(Priority, Source.GetSchedulingPriority());

		int32 Step = 1;
		bool bFloor = false;
		if (!VoxelStreamingGeometry::ResolveSampleStep(FMath::Max(Distance, double(Entry.MinimumSamplingDistanceCells)), Source.VerticalFovDegrees, Source.ViewportHeightPixels, InTargetPixels,
				static_cast<int32>(InKey.SideCells()), Step, bFloor, OutError,
				InKey.Representation == EVoxelTerrainRepresentation::VoxelProxy ? 2 : 1))
		{
			return false;
		}

		// 多 Source 相交时使用最严格（最细）的采样要求。
		Sampling = FMath::Min(Sampling, Step);
		bResolutionFloor |= bFloor;
	}

	if (!bRelevant)
	{
		return true;
	}

	const uint8 MinimumLevel = InKey.IsColumn() ? 0 : 1;
	const int32 PreferredSamples = InKey.IsColumn() ? 64 : 32;
	const uint8 ScreenDesired = VoxelStreamingGeometry::ResolveCoverageLevel(Sampling, PreferredSamples, {MinimumLevel, InMaximumLevel});

	const uint8 WorkMaximum = ResolveWorkBoundedMaximumLevel(Sampling, InMaximumSamplesPerAxis, InMaximumLevel);

	uint8 LocalPredecessorPreference = 0;
	if (!ResolveLocalPredecessorPreference(InKey, InWorldMinZ, InWorldMaxZ, InPredecessorCoverage, LocalPredecessorPreference, OutError)) return false;

	// Predecessor 只提供局部空间连续性偏好；绝不突破工作量硬上限。
	const uint8 PreferredWithPredecessor = FMath::Max(ScreenDesired, FMath::Min(LocalPredecessorPreference, WorkMaximum));
	const uint8 Desired = FMath::Min(FMath::Min(PreferredWithPredecessor, WorkMaximum), InMaximumPageLevel);

	const FVoxelTerrainPageCost Cost = FVoxelTerrainPageCost::Estimate(InKey.SideCells(), Sampling, InKey.IsColumn());
	const bool bCostExceeded = !InCostLimits.Accepts(Cost);
	if (InKey.Level > Desired || (bCostExceeded && InKey.Level > MinimumLevel))
	{
		// 父节点分区 scratch 不沿递归路径累计保活。
		Contributions.Empty();
		Next.Empty();
		ContributionLease.Reset();
		for (int32 Child = 0; Child < (InKey.IsColumn() ? 4 : 8); ++Child)
		{
			FVoxelTerrainSpatialKey ChildKey = InKey;
			ChildKey.Level = InKey.Level - 1;
			ChildKey.Coordinate = InKey.Coordinate * 2 + FIntVector(Child & 1, (Child >> 1) & 1, InKey.IsColumn() ? 0 : ((Child >> 2) & 1));

			if (!Visit(ChildKey, InSources, InMaximumLevel, InMaximumSamplesPerAxis, InWorldMinZ, InWorldMaxZ, InTargetPixels, InGeneratorIdentity,
					InMaterialRevision, InOutPlan, InOutVisited, OutError, InPredecessorCoverage, InCostLimits, InCache,
					InPrevious ? InPrevious->Children[Child] : nullptr, CacheNode ? &CacheNode->Children[Child] : nullptr, InMaximumPageLevel))
			{
				return false;
			}
		}
		return true;
	}

	if (bCostExceeded || InKey.Level < MinimumLevel)
	{
		OutError =
			FString::Printf(TEXT("Terrain minimum page blocked by cost policy: representation=%d level=%u samples=%llu triangles=%llu bytes=%llu buildMs=%.3f"),
				int32(InKey.Representation), InKey.Level, Cost.Samples, Cost.Triangles, Cost.MeshBytes, Cost.BuildMilliseconds);
		return false;
	}
	const int64 SamplesPerAxis = InKey.SideCells() / Sampling;
	if (SamplesPerAxis > InMaximumSamplesPerAxis)
	{
		OutError = FString::Printf(TEXT("Terrain page violates the work bound: representation=%d level=%u side=%lld step=%d samples=%lld max=%d"),
			static_cast<int32>(InKey.Representation), InKey.Level, InKey.SideCells(), Sampling, SamplesPerAxis, InMaximumSamplesPerAxis);
		return false;
	}

	FVoxelTerrainGeometry Geometry{InKey, Sampling, InWorldMinZ, InWorldMaxZ, InGeneratorIdentity, InMaterialRevision};

	if (!Geometry.Validate(OutError))
	{
		return false;
	}

	if (!AppendStagePage(InOutPlan, Geometry, Cost, Priority, bResolutionFloor, OutError)) return false;
	if (CacheNode)
	{
		CacheNode->bPage = true; CacheNode->Geometry = Geometry; CacheNode->Cost = Cost;
		CacheNode->Priority = Priority; CacheNode->bFloor = bResolutionFloor;
	}
	return true;
}

FVoxelTerrainPageCost FVoxelTerrainPageCost::Estimate(const int64 InSideCells, const int32 InStepCells, const bool bColumn)
{
	const uint64 Side = InSideCells / FMath::Max(1, InStepCells);
	FVoxelTerrainPageCost Result;
	if (Side > 65536 || (!bColumn && Side > 65536 / FMath::Max<uint64>(1, Side)))
	{
		Result.Samples = Result.Triangles = Result.MeshBytes = MAX_uint64;
		Result.BuildMilliseconds = MAX_dbl;
		return Result;
	}
	Result.Samples = Side * Side * (bColumn ? 1 : Side);
	Result.Triangles = Result.Samples * (bColumn ? 4 : 12);
	Result.MeshBytes = Result.Triangles * (3 * 80 + 3 * sizeof(uint32));
	Result.BuildMilliseconds = double(Result.Samples) * (bColumn ? 0.0001 : 0.0002);
	return Result;
}

bool FVoxelTerrainPageCostLimits::Accepts(const FVoxelTerrainPageCost& InCost) const
{
	return InCost.Triangles <= Triangles && InCost.MeshBytes <= MeshBytes && InCost.BuildMilliseconds <= BuildMilliseconds;
}
