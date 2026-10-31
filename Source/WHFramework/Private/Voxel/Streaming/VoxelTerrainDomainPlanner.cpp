#include "Voxel/Streaming/VoxelTerrainDomainPlanner.h"

#include "Voxel/Rendering/VoxelCoverageIndex.h"
#include "Voxel/Rendering/VoxelCoveragePartition.h"
#include "Voxel/Rendering/VoxelTerrainStagePlanner.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Misc/ScopeExit.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

void FVoxelTerrainDomainPlanner::BindResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer)
{
	if (Stages[0].Resources.Get() != &InResources.Get() || Stages[0].Consumer != InConsumer) bCapacityBlocked = false;
	for (auto& Stage : Stages)
	{
		if (Stage.Resources.Get() != &InResources.Get() || Stage.Consumer != InConsumer)
		{ Stage.bCapacityBlocked = false; Stage.bTerminalAttempt = false; Stage.TerminalSources.Reset(); Stage.TerminalPredecessor.Reset(); }
		Stage.Resources = InResources;
		Stage.Consumer = InConsumer;
	}
}

bool FVoxelTerrainDomainPlanner::ApplySourceDelta(const EVoxelTerrainRepresentation InRepresentation,
	TConstArrayView<FVoxelTerrainStageSource> InSources, FString& OutError)
{
	const int32 Stage = int32(InRepresentation) - int32(EVoxelTerrainRepresentation::VoxelProxy);
	if (Stage < 0 || Stage >= 3) { OutError = TEXT("Invalid coarse domain"); return false; }
	const auto SameSource = [](const FVoxelTerrainStageSource& Left, const FVoxelTerrainStageSource& Right)
	{
		return Left.Source.Id == Right.Source.Id && Left.Source.Center == Right.Source.Center &&
			Left.Source.VerticalFovDegrees == Right.Source.VerticalFovDegrees &&
			Left.Source.ViewportHeightPixels == Right.Source.ViewportHeightPixels &&
			Left.Source.GetSchedulingPriority() == Right.Source.GetSchedulingPriority() &&
			Left.OuterRadiusCells == Right.OuterRadiusCells && Left.InnerRadiusCells == Right.InnerRadiusCells &&
			Left.VerticalHalfExtentCells == Right.VerticalHalfExtentCells && Left.MinimumSamplingDistanceCells == Right.MinimumSamplingDistanceCells;
	};
	bool bSameSources = Sources[Stage] && Sources[Stage]->Values.Num() == InSources.Num();
	for (int32 Index = 0; bSameSources && Index < InSources.Num(); ++Index)
		bSameSources = SameSource(Sources[Stage]->Values[Index], InSources[Index]);
	if (bSameSources) { Stages[Stage].SourceInputs = Sources[Stage]; OutError.Reset(); return true; }
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainDomainSourceBatch>() +
		VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelTerrainStageSource, FDefaultAllocator>(InSources.Num());
	TSharedPtr<FVoxelTerrainDomainSourceBatch, ESPMode::ThreadSafe> Candidate;
	if (Stages[Stage].Resources)
	{
		const uint64 BeforeAdmission = Stages[Stage].Resources->ReleaseVersionExcludingOwner(Stages[Stage].Consumer);
		uint64 Input = AdditionalRequiredInput; int32 Count = AdditionalRequiredCount;
		if (!AccumulateRetainedInput(*Stages[Stage].Resources, nullptr, {}, -1, Input, Count))
		{ OutError = TEXT("CapacityUnachievable: coarse source retained input"); return false; }
		const auto Admission = Stages[Stage].Resources->TryAcquire(Stages[Stage].Consumer,
			EVoxelTerrainResourcePool::Input, Bytes, Lease, 0, Input, Count);
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			Stages[Stage].bCapacityBlocked = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked;
			Stages[Stage].RetryCapacityVersion = BeforeAdmission;
			OutError = Stages[Stage].bCapacityBlocked ? TEXT("CapacityBlocked: coarse source inputs") : TEXT("CapacityUnachievable: coarse source inputs");
			return false;
		}
		Candidate = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainDomainSourceBatch>(Lease);
	}
	else Candidate = MakeShared<FVoxelTerrainDomainSourceBatch, ESPMode::ThreadSafe>();
	Candidate->Lease = MoveTemp(Lease); Candidate->Values.Reserve(InSources.Num()); Candidate->Values.Append(InSources);
	Sources[Stage] = MoveTemp(Candidate);
	Stages[Stage].SourceInputs = Sources[Stage];
	Stages[Stage].bCapacityBlocked = false;
	OutError.Reset();
	return true;
}

bool FVoxelTerrainDomainPlanner::AdvanceDirtyRoots(const EVoxelTerrainRepresentation InRepresentation, const uint8 InMaximumLevel,
	const int32 InMaximumSamples, const int32 InMinZ, const int32 InMaxZ, const double InTargetPixels,
	const uint64 InGenerator, const uint64 InMaterial, FVoxelTerrainStagePlan& OutPlan, FString& OutError,
	const FVoxelCoverageIndex* InPredecessor, const FVoxelTerrainPageCostLimits& InLimits)
{
	const int32 Stage = int32(InRepresentation) - int32(EVoxelTerrainRepresentation::VoxelProxy);
	if (Stage < 0 || Stage >= 3 || !Sources[Stage]) { OutError = TEXT("Missing frozen coarse source delta"); return false; }
	OutPlan.RetainedInputBytes = AdditionalRequiredInput; OutPlan.RetainedWorkingBytes = AdditionalRequiredWorking;
	OutPlan.RetainedLeaseCount = AdditionalRequiredCount;
	if (Stages[Stage].Resources && !AccumulateRetainedInput(*Stages[Stage].Resources, nullptr, {}, Stage,
		OutPlan.RetainedInputBytes, OutPlan.RetainedLeaseCount))
	{ OutError = TEXT("CapacityUnachievable: coarse stage frozen root union"); return false; }
	Stages[Stage].bExternalCoverageFloor = true;
	ON_SCOPE_EXIT { Stages[Stage].bExternalCoverageFloor = false; };
	return FVoxelTerrainStagePlanner::AdvanceDirtyRoots(InRepresentation, InMaximumLevel, InMaximumSamples,
		InMinZ, InMaxZ, InTargetPixels, InGenerator, InMaterial, OutPlan, OutError, InPredecessor, InLimits, Stages[Stage], 64,
		PrefixAppliedVersions[Stage] != Stages[Stage].GetOutputVersion());
}

void FVoxelTerrainDomainPlanner::Reset()
{
	FinePredecessor.Reset();
	for (auto& Predecessor : Predecessors) Predecessor.Reset();
	for (auto& Version : PrefixAppliedVersions) Version = 0;
	for (auto& Source : Sources) Source.Reset();
	for (auto& Stage : Stages) Stage = {};
	bCapacityBlocked = false;
	RetryCapacityVersion = 0;
}

bool FVoxelTerrainDomainPlanner::HasPendingWork() const
{
	if (bCapacityBlocked) return true;
	for (const auto& Stage : Stages) if (Stage.HasPendingWork()) return true;
	return false;
}

bool FVoxelTerrainDomainPlanner::HasRetryReady() const
{
	if (bCapacityBlocked && Stages[0].Resources)
	{
		uint64 Version = 0;
		return Stages[0].Resources->TryGetReleaseVersionExcludingOwner(Stages[0].Consumer, Version) && Version > RetryCapacityVersion;
	}
	for (const auto& Stage : Stages)
		if (Stage.IsRetryReady() || (Stage.HasPendingWork() && !Stage.bCapacityBlocked)) return true;
	return false;
}

bool FVoxelTerrainDomainPlanner::AccumulateRetainedInput(const FVoxelTerrainResources& InResources,
	const FVoxelTerrainDomainPlanner* InOther, TConstArrayView<uint64> InExcludedSerials, const int32 InSkipStage,
	uint64& InOutBytes, int32& InOutLeaseCount) const
{
	const FVoxelCoverageIndex* Coverage[32] = {&FinePredecessor, InOther ? &InOther->FinePredecessor : nullptr};
	int32 CoverageCount = 2;
	for (int32 Stage = 0; Stage < 3; ++Stage)
	{
		Coverage[CoverageCount++] = &Predecessors[Stage];
		if (InOther) Coverage[CoverageCount++] = &InOther->Predecessors[Stage];
	}
	for (int32 Stage = 0; Stage < 3; ++Stage)
	{
		if (Stage != InSkipStage && !Stages[Stage].AccumulateRetainedInput(InResources,
			InOther ? &InOther->Stages[Stage] : nullptr, InExcludedSerials, InOutBytes, InOutLeaseCount, false)) return false;
		const auto AddCoverage = [&](const FVoxelCoverageIndex& Index)
		{ check(CoverageCount < UE_ARRAY_COUNT(Coverage)); Coverage[CoverageCount++] = &Index; };
		Stages[Stage].VisitRetainedCoverage(AddCoverage);
		if (InOther) InOther->Stages[Stage].VisitRetainedCoverage(AddCoverage);
	}
	return FVoxelCoverageIndex::AccumulateRetainedInputs(InResources, MakeArrayView(Coverage, CoverageCount),
		InExcludedSerials, InOutBytes, InOutLeaseCount);
}

namespace
{
	struct FDomainSourceSideUpdate
	{
		FGuid Source;
		int32 Stage = 0;
		int32 SideCells = 0;
	};

	struct FDomainCompositionFloor
	{
		bool Add(const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease,
			const FVoxelTerrainResources& Resources)
		{
			if (!Lease || !Lease->IsOwnedBy(Resources)) return true;
			for (int32 Index = 0; Index < LeaseCount; ++Index) if (Leases[Index] == Lease.Get()) return true;
			check(LeaseCount < UE_ARRAY_COUNT(Leases));
			const uint64 Input = Lease->GetPool() == EVoxelTerrainResourcePool::Input ? Lease->Bytes() : 0;
			if (Input > MAX_uint64 - Bytes) return false;
			Bytes += Input;
			if (Lease->GetPool() == EVoxelTerrainResourcePool::Working) WorkingBytes += Lease->Bytes();
			Leases[LeaseCount++] = Lease.Get();
			return true;
		}
		uint64 Bytes = 0;
		uint64 WorkingBytes = 0;
		int32 LeaseCount = 0;
		const FVoxelTerrainResourceLease* Leases[18] = {};
	};

	bool IntersectsSource(const FVoxelTerrainGeometry& InGeometry, const FVoxelTerrainStageSource& InSource)
	{
		FIntVector Min, Max;
		if (!InGeometry.Key.TryBounds(InGeometry.WorldMinZ, InGeometry.WorldMaxZ, Min, Max)) return false;
		const auto& Center = InSource.Source.Center;
		const double X = Center.X - FMath::Clamp(Center.X, Min.X, Max.X);
		const double Y = Center.Y - FMath::Clamp(Center.Y, Min.Y, Max.Y);
		return X * X + Y * Y <= FMath::Square(double(InSource.OuterRadiusCells)) &&
			(InGeometry.Key.IsColumn() || (Max.Z > int64(Center.Z) - InSource.VerticalHalfExtentCells && Min.Z <= int64(Center.Z) + InSource.VerticalHalfExtentCells));
	}

	int32 ResolveActualFirstSideCells(const FVoxelTerrainStageSource& InSource, const TMap<FVoxelTerrainSpatialKey, FVoxelTerrainGeometry>& InPages)
	{
		int32 Result = MAX_int32;
		for (const auto& Pair : InPages)
		{
			if (IntersectsSource(Pair.Value, InSource))
			{
				Result = FMath::Min(Result, static_cast<int32>(Pair.Key.SideCells()));
			}
		}

		return Result == MAX_int32 ? 16 : Result;
	}
}

bool FVoxelTerrainDomainPlanner::Build(TConstArrayView<FVoxelStreamingSource> InSources, const FVoxelWorldManifest& InManifest,
	const FVoxelViewSettings& InSettings, FVoxelInterestSet& InOutInterest, FString& OutError, FVoxelTerrainDomainPlanner* InPersistent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_TerrainDomainBuild);
	check(InOutInterest.FinePagePlan);
	const auto Resources = InPersistent ? InPersistent->Stages[0].Resources : nullptr;
	const auto Consumer = InPersistent ? InPersistent->Stages[0].Consumer : FVoxelGenerationConsumerKey{};
	if (InPersistent && InPersistent->bCapacityBlocked && !InPersistent->HasRetryReady())
	{
		OutError = TEXT("CapacityBlocked: coarse composition awaiting external release");
		return false;
	}
	const uint64 BeforeBuild = Resources ? Resources->ReleaseVersionExcludingOwner(Consumer) : 0;
	FVoxelTerrainDomainPlanner CandidatePlanner = InPersistent ? *InPersistent : FVoxelTerrainDomainPlanner();
	bool bComplete = false;
	ON_SCOPE_EXIT
	{
		CandidatePlanner.FinePredecessor.SetAdditionalRequiredInput(0, 0);
		for (auto& Prefix : CandidatePlanner.Predecessors) Prefix.SetAdditionalRequiredInput(0, 0);
		if (!bComplete && InPersistent)
		{
			CandidatePlanner.AdditionalRequiredInput = CandidatePlanner.AdditionalRequiredWorking = 0;
			CandidatePlanner.AdditionalRequiredCount = 0;
			CandidatePlanner.bCapacityBlocked = OutError.StartsWith(TEXT("CapacityBlocked"));
			CandidatePlanner.RetryCapacityVersion = BeforeBuild;
			*InPersistent = MoveTemp(CandidatePlanner);
		}
	};
	if (int64(InSources.Num()) * 3 > MAX_int32)
	{
		OutError = TEXT("CapacityUnachievable: coarse source composition count");
		return false;
	}
	FDomainCompositionFloor InterestFloor;
	if (Resources)
	{
		bool bValid = InterestFloor.Add(InOutInterest.DemandLease, *Resources) && InterestFloor.Add(InOutInterest.CompositionLease, *Resources) &&
			InterestFloor.Add(InOutInterest.CopyLease, *Resources) && InterestFloor.Add(InOutInterest.FinalizationLease, *Resources) &&
			InterestFloor.Add(InOutInterest.CoarseCompositionLease, *Resources);
		for (int32 Stage = 0; Stage < 3; ++Stage)
		{
			bValid &= InterestFloor.Add(InOutInterest.CoarsePlanLeases[Stage], *Resources);
		}
		if (!bValid) { OutError = TEXT("CapacityUnachievable: coarse retained original inputs"); return false; }
	}
	const auto BindGraphFloor = [&](FDomainCompositionFloor& Floor, const int32 SkipStage = -1)
	{
		if (!Resources) return true;
		uint64 Excluded[18] = {};
		for (int32 I = 0; I < Floor.LeaseCount; ++I) Excluded[I] = Floor.Leases[I]->Serial();
		return CandidatePlanner.AccumulateRetainedInput(*Resources, InPersistent, MakeArrayView(Excluded, Floor.LeaseCount),
			SkipStage, Floor.Bytes, Floor.LeaseCount);
	};
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> SourceScratch;
	TArray<FDomainSourceSideUpdate> SourceSideUpdates;
	const uint64 SourceBytes = FMath::Max<uint64>(1,
		VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelTerrainStageSource, FDefaultAllocator>(InSources.Num()) +
		VoxelGenerationAllocation::Private::ArrayReserveBytes<FDomainSourceSideUpdate, FDefaultAllocator>(InSources.Num() * 3));
	if (Resources)
	{
		const uint64 Limit = Resources->Snapshot().Limits.WorkingBytes;
		if (SourceBytes > Limit || InterestFloor.WorkingBytes > Limit - SourceBytes)
		{ OutError = TEXT("CapacityUnachievable: coarse Source retained Working peak"); return false; }
		FDomainCompositionFloor Floor = InterestFloor;
		if (!BindGraphFloor(Floor)) { OutError = TEXT("CapacityUnachievable: coarse Source frozen cache input"); return false; }
		const auto Admission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Working, SourceBytes, SourceScratch,
			0, Floor.Bytes, Floor.LeaseCount);
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			OutError = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: coarse Source scratch") :
				TEXT("CapacityUnachievable: coarse Source scratch");
			return false;
		}
	}
	SourceSideUpdates.Reserve(InSources.Num() * 3);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CompositionLease;
	TMap<FVoxelTerrainSpatialKey, FVoxelTerrainGeometry> Geometry;
	TSet<FVoxelTerrainSpatialKey> ResolutionFloor;
	TSet<FVoxelViewKey> VoxelProxy;
	TSet<FVoxelSurfaceTileKey> Surface;
	TSet<FVoxelMacroTileKey> Macro;
	int32 FailedRootCount = 0;
	const auto ReserveOutput = [&](const int64 InGeometryRows, const int64 InFloorRows,
		const int64 InProxyRows, const int64 InSurfaceRows, const int64 InMacroRows,
		const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& TransientLease = nullptr)
	{
		if (InGeometryRows > MAX_int32 || InFloorRows > MAX_int32 || InProxyRows > MAX_int32 ||
			InSurfaceRows > MAX_int32 || InMacroRows > MAX_int32)
		{ OutError = TEXT("Oversized: coarse composed output rows"); return false; }
		const uint64 OldParts[] = {VoxelGenerationAllocation::AllocatedBytes(Geometry), VoxelGenerationAllocation::AllocatedBytes(ResolutionFloor),
			VoxelGenerationAllocation::AllocatedBytes(VoxelProxy), VoxelGenerationAllocation::AllocatedBytes(Surface), VoxelGenerationAllocation::AllocatedBytes(Macro)};
		const uint64 NewParts[] = {
			VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FVoxelTerrainSpatialKey, FVoxelTerrainGeometry>>(Geometry, int32(InGeometryRows)),
			VoxelGenerationAllocation::Private::SetReserveBytes<FVoxelTerrainSpatialKey>(ResolutionFloor, int32(InFloorRows)),
			VoxelGenerationAllocation::Private::SetReserveBytes<FVoxelViewKey>(VoxelProxy, int32(InProxyRows)),
			VoxelGenerationAllocation::Private::SetReserveBytes<FVoxelSurfaceTileKey>(Surface, int32(InSurfaceRows)),
			VoxelGenerationAllocation::Private::SetReserveBytes<FVoxelMacroTileKey>(Macro, int32(InMacroRows))};
		const bool Growing[] = {
			!VoxelGenerationAllocation::Private::SetCanReserve<TPair<FVoxelTerrainSpatialKey, FVoxelTerrainGeometry>>(Geometry, int32(InGeometryRows)),
			!VoxelGenerationAllocation::Private::SetCanReserve<FVoxelTerrainSpatialKey>(ResolutionFloor, int32(InFloorRows)),
			!VoxelGenerationAllocation::Private::SetCanReserve<FVoxelViewKey>(VoxelProxy, int32(InProxyRows)),
			!VoxelGenerationAllocation::Private::SetCanReserve<FVoxelSurfaceTileKey>(Surface, int32(InSurfaceRows)),
			!VoxelGenerationAllocation::Private::SetCanReserve<FVoxelMacroTileKey>(Macro, int32(InMacroRows))};
		uint64 NewBytes = 0, Current = 0;
		bool bGrowth = false;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(OldParts); ++Index)
		{
			if (NewParts[Index] == MAX_uint64 || NewParts[Index] > MAX_uint64 - NewBytes || OldParts[Index] > MAX_uint64 - Current)
			{ OutError = TEXT("CapacityUnachievable: coarse composed allocator capacity"); return false; }
			NewBytes += NewParts[Index];
			Current += OldParts[Index];
			bGrowth |= Growing[Index];
		}
		if (!bGrowth) return true;
		uint64 PeakBytes = Current;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(OldParts); ++Index)
		{
			if (!Growing[Index]) continue;
			if (NewParts[Index] > MAX_uint64 - Current)
			{ OutError = TEXT("CapacityUnachievable: coarse composed allocator peak"); return false; }
			PeakBytes = FMath::Max(PeakBytes, Current + NewParts[Index]);
			Current = Current - OldParts[Index] + NewParts[Index];
		}
		PeakBytes = FMath::Max<uint64>(1, PeakBytes);
		if (Resources)
		{
			FDomainCompositionFloor Floor = InterestFloor;
			bool bValidFloor = Floor.Add(SourceScratch, *Resources) && Floor.Add(TransientLease, *Resources);
			bValidFloor &= BindGraphFloor(Floor);
			if (!bValidFloor) { OutError = TEXT("CapacityUnachievable: coarse composition retained input"); return false; }
			const auto Admission = CompositionLease ? CompositionLease->TryResize(PeakBytes, Floor.Bytes, Floor.LeaseCount) :
				Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input, PeakBytes, CompositionLease, 0, Floor.Bytes, Floor.LeaseCount);
			if (Admission != EVoxelTerrainResourceAdmission::Granted)
			{
				OutError = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: coarse composed allocation peak") :
					TEXT("CapacityUnachievable: coarse composed allocation peak");
				return false;
			}
		}
		Geometry.Reserve(int32(InGeometryRows)); ResolutionFloor.Reserve(int32(InFloorRows));
		VoxelProxy.Reserve(int32(InProxyRows)); Surface.Reserve(int32(InSurfaceRows)); Macro.Reserve(int32(InMacroRows));
		if (Resources)
		{
			const uint64 Actual = VoxelGenerationAllocation::AllocatedBytes(Geometry) + VoxelGenerationAllocation::AllocatedBytes(ResolutionFloor) +
				VoxelGenerationAllocation::AllocatedBytes(VoxelProxy) + VoxelGenerationAllocation::AllocatedBytes(Surface) + VoxelGenerationAllocation::AllocatedBytes(Macro);
			check(Actual <= NewBytes);
			const bool bResized = CompositionLease->Resize(FMath::Max<uint64>(1, Actual));
			check(bResized);
		}
		return true;
	};
	const auto ComposeFine = [&]()
	{
		if (!ReserveOutput(InOutInterest.FinePagePlan->Pages.Num(), 0, 0, 0, 0)) return false;
		for (const FVoxelFinePageKey& Page : InOutInterest.FinePagePlan->Pages)
		{
			const FVoxelTerrainSpatialKey Key{EVoxelTerrainRepresentation::Fine, Page.Coordinate, Page.Level};
			Geometry.Add(Key, {Key, 1, InManifest.Settings.MinZ, InManifest.Settings.MaxZ, InManifest.BaseSampleHash, InManifest.RegistryHash});
		}

		return true;
	};
	if (!InPersistent && !ComposeFine()) return false;

	const FVoxelCoverageIndex OldFinePredecessor = CandidatePlanner.FinePredecessor;
	if (Resources)
	{
		CandidatePlanner.FinePredecessor.BindResources(Resources.ToSharedRef(), Consumer);
		for (auto& Prefix : CandidatePlanner.Predecessors) Prefix.BindResources(Resources.ToSharedRef(), Consumer);
	}
	const auto PrepareCoverageFloor = [&](FVoxelCoverageIndex& Index, const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Extra = nullptr)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_TerrainDomainCoverageFloor);
		if (!Resources) return true;
		FDomainCompositionFloor Floor = InterestFloor;
		if (!Floor.Add(SourceScratch, *Resources) || !Floor.Add(CompositionLease, *Resources) || !Floor.Add(Extra, *Resources) || !BindGraphFloor(Floor))
		{ OutError = TEXT("CapacityUnachievable: coarse predecessor external retained floor"); return false; }
		uint64 OwnBytes = 0; int32 OwnCount = 0;
		if (!Index.AccumulateRetainedInput(*Resources, nullptr, {}, OwnBytes, OwnCount) || OwnBytes > Floor.Bytes || OwnCount > Floor.LeaseCount)
		{ OutError = TEXT("CapacityUnachievable: coarse predecessor own root floor"); return false; }
		Index.SetAdditionalRequiredInput(Floor.Bytes - OwnBytes, Floor.LeaseCount - OwnCount);
		return true;
	};
	// 冻结旧根后遍历删除；更新保持局部 COW，不改工作线程已持有的前驱事实。
	bool FineReady = true;
	OldFinePredecessor.AnyRecord(FIntVector(MIN_int32), FIntVector(MAX_int32), [&](const FVoxelCoverageRecord& Record)
	{
		const FVoxelFineShardKey Shard{Record.Min, int32(Record.Geometry.Key.SideCells())};
		if (InOutInterest.FineShardDemands.Contains(Shard)) return false;
		FineReady = PrepareCoverageFloor(CandidatePlanner.FinePredecessor) && CandidatePlanner.FinePredecessor.Remove(Record.Geometry.Key, OutError);
		return !FineReady;
	});
	if (!FineReady) return false;
	for (const auto& Pair : InOutInterest.FineShardDemands)
	{
		const auto& Shard = Pair.Key;
		const FVoxelTerrainSpatialKey Key{EVoxelTerrainRepresentation::Fine, Shard.Min / Shard.SideCells,
			uint8(FMath::FloorLog2(uint32(Shard.SideCells / 16)))};
		FVoxelCoverageRecord Record;
		Record.Geometry = {Key, 1, InManifest.Settings.MinZ, InManifest.Settings.MaxZ, InManifest.BaseSampleHash, InManifest.RegistryHash};
		if (!Key.TryBounds(InManifest.Settings.MinZ, InManifest.Settings.MaxZ, Record.Min, Record.Max)) return false;
		if (CandidatePlanner.FinePredecessor.HasMatchingRecord(Record)) continue;
		if (!PrepareCoverageFloor(CandidatePlanner.FinePredecessor) || !CandidatePlanner.FinePredecessor.Upsert(Record, OutError)) return false;
	}
	CandidatePlanner.FinePredecessor.SetAdditionalRequiredInput(0, 0);
	CandidatePlanner.Predecessors[0] = CandidatePlanner.FinePredecessor;
	for (int32 Stage = 1; Stage < 3; ++Stage)
	{
		// 空前驱直接共享已冻结 Fine 根，不逐条复制成独立但等价的树。
		if (CandidatePlanner.Predecessors[Stage].NumRecords() == 0)
		{
			CandidatePlanner.Predecessors[Stage] = CandidatePlanner.FinePredecessor;
			continue;
		}
		bool Ready = true;
		CandidatePlanner.FinePredecessor.VisitChangedRecords(OldFinePredecessor, [&](const auto* Now, const auto* Old)
		{
			if (!Ready) return;
			auto& Prefix = CandidatePlanner.Predecessors[Stage];
			Ready = PrepareCoverageFloor(Prefix) && (Now ? Prefix.Upsert(*Now, OutError) : Prefix.Remove(Old->Geometry.Key, OutError));
		});
		CandidatePlanner.Predecessors[Stage].SetAdditionalRequiredInput(0, 0);
		if (!Ready) return false;
	}
	const auto AddCoverage = [&](const EVoxelTerrainRepresentation Representation, const FVoxelTerrainStagePlan& Plan)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_TerrainDomainAddCoverage);
		const int32 FirstStage = int32(Representation) - int32(EVoxelTerrainRepresentation::VoxelProxy) + 1;
		for (int32 Stage = FirstStage; Stage < 3; ++Stage)
		{
			auto& Prefix = CandidatePlanner.Predecessors[Stage];
			const FVoxelCoverageIndex Old = Prefix;
			bool Ready = true;
			Old.AnyRecord(FIntVector(MIN_int32), FIntVector(MAX_int32), [&](const FVoxelCoverageRecord& Record)
			{
				if (Record.Geometry.Key.Representation != Representation || Plan.Pages.Contains(Record.Geometry.Key)) return false;
				Ready = PrepareCoverageFloor(Prefix, Plan.ResourceLease) && Prefix.Remove(Record.Geometry.Key, OutError); return !Ready;
			});
			if (!Ready) return false;
			for (const auto& Pair : Plan.Pages)
			{
				FVoxelCoverageRecord Record; Record.Geometry = Pair.Value;
				if (!Pair.Key.TryBounds(Pair.Value.WorldMinZ, Pair.Value.WorldMaxZ, Record.Min, Record.Max)) return false;
				if (Prefix.HasMatchingRecord(Record)) continue;
				if (!PrepareCoverageFloor(Prefix, Plan.ResourceLease) || !Prefix.Upsert(Record, OutError)) return false;
			}
			Prefix.SetAdditionalRequiredInput(0, 0);
		}
		return true;
	};

	const auto ComposeStage = [&](const EVoxelTerrainRepresentation Representation, const FVoxelTerrainStagePlan& Plan,
		TConstArrayView<FVoxelTerrainStageSource> StageSources)
	{
		for (const FVoxelTerrainStageSource& StageSource : StageSources)
		{
			if (!InOutInterest.Sources.Contains(StageSource.Source.Id))
			{
				OutError = TEXT("Missing accepted coarse Source input");
				return false;
			}
			SourceSideUpdates.Add({StageSource.Source.Id, static_cast<int32>(Representation) - 2,
				ResolveActualFirstSideCells(StageSource, Plan.Pages)});
		}
		const int64 NewRows = Plan.Pages.Num();
		if (!ReserveOutput(int64(Geometry.Num()) + NewRows, int64(ResolutionFloor.Num()) + Plan.ResolutionFloorPages.Num(),
			int64(VoxelProxy.Num()) + (Representation == EVoxelTerrainRepresentation::VoxelProxy ? NewRows : 0),
			int64(Surface.Num()) + (Representation == EVoxelTerrainRepresentation::Surface ? NewRows : 0),
			int64(Macro.Num()) + (Representation == EVoxelTerrainRepresentation::Macro ? NewRows : 0), Plan.ResourceLease)) return false;
		Geometry.Append(Plan.Pages);
		FailedRootCount += Plan.FailedRootCount;
		ResolutionFloor.Append(Plan.ResolutionFloorPages);

		for (const auto& Pair : Plan.Pages)
		{
			const FVoxelTerrainSpatialKey& Key = Pair.Key;
			if (Representation == EVoxelTerrainRepresentation::VoxelProxy)
			{
				VoxelProxy.Add({Key.Coordinate, Key.Level});
			}
			else if (Representation == EVoxelTerrainRepresentation::Surface)
			{
				Surface.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
			}
			else
			{
				Macro.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
			}
		}
		return true;
	};

	for (const EVoxelTerrainRepresentation Representation :
		{EVoxelTerrainRepresentation::VoxelProxy, EVoxelTerrainRepresentation::Surface, EVoxelTerrainRepresentation::Macro})
	{
		TArray<FVoxelTerrainStageSource> Sources;
		Sources.Reserve(InSources.Num());

		for (const FVoxelStreamingSource& InputSource : InSources)
		{
			FVoxelStreamingSource Source = InputSource;
			const int32 StageIndex = static_cast<int32>(Representation) - 2;

			const auto* Accepted = InOutInterest.Sources.Find(Source.Id);
			if (Accepted && Accepted->bHasStageCenters)
			{
				Source.Center = Accepted->StageCenters[StageIndex];
			}

			const bool bLocal = Source.Has(EVoxelStreamingCapability::LocalRefinement) && !Source.Has(EVoxelStreamingCapability::WorldVisual);

			if (!Source.Has(EVoxelStreamingCapability::WorldVisual) && !(bLocal && Representation == EVoxelTerrainRepresentation::VoxelProxy))
			{
				continue;
			}

			const FVoxelResolvedViewRanges Ranges = VoxelViewLod::ResolveViewRanges(Source);

			if ((Representation == EVoxelTerrainRepresentation::VoxelProxy && !bLocal && Source.View.VoxelProxyRadiusCells <= 0) ||
				(Representation == EVoxelTerrainRepresentation::Surface && Source.View.SurfaceRadiusCells <= 0) ||
				(Representation == EVoxelTerrainRepresentation::Macro && Source.View.MacroRadiusCells <= 0))
			{
				continue;
			}

			int32 LocalOuter = Ranges.Fine + Source.View.InitialChunkSideCells * 2;
			if (bLocal && Accepted && Accepted->FineBounds.IsValid())
			{
				const auto& Bounds = Accepted->FineBounds;
				const double X = FMath::Max(FMath::Abs(double(Bounds.Min.X) - Source.Center.X),
					FMath::Abs(double(Bounds.Max.X) - Source.Center.X));
				const double Y = FMath::Max(FMath::Abs(double(Bounds.Min.Y) - Source.Center.Y),
					FMath::Abs(double(Bounds.Max.Y) - Source.Center.Y));
				const double Radius = FMath::CeilToDouble(FMath::Sqrt(X * X + Y * Y)) + double(Source.View.InitialChunkSideCells) * 2.0;
				if (Radius > MAX_int32) { OutError = TEXT("Oversized: local Proxy residency envelope"); return false; }
				LocalOuter = int32(Radius);
			}
			const int32 Outer = Representation == EVoxelTerrainRepresentation::VoxelProxy
									? (bLocal ? LocalOuter : Ranges.Proxy)
								: Representation == EVoxelTerrainRepresentation::Surface ? Ranges.Surface
																						 : Ranges.Macro;

			const int32 PreviousOuter = Source.View.SurfaceRadiusCells > 0		? Ranges.Surface
										: Source.View.VoxelProxyRadiusCells > 0 ? Ranges.Proxy
																				: Ranges.Fine;

			const int32 Inner = Representation == EVoxelTerrainRepresentation::Macro ? PreviousOuter : 0;

			if (Outer <= Inner || Outer <= 0)
			{
				continue;
			}

			int32 SamplingFloor = 0;
			if (Representation == EVoxelTerrainRepresentation::Surface)
				SamplingFloor = Source.View.VoxelProxyRadiusCells > 0 ? Ranges.Proxy :
					Source.Has(EVoxelStreamingCapability::FineVisual) ? Ranges.Fine : 0;
			else if (Representation == EVoxelTerrainRepresentation::Macro) SamplingFloor = PreviousOuter;
			Sources.Add({Source, Outer,
				Representation == EVoxelTerrainRepresentation::VoxelProxy ? Source.View.VoxelProxyVerticalHalfExtentCells : (MAX_int32 / 2),
				Inner, FMath::Min(Outer, SamplingFloor)});
		}

		const uint8 Maximum = Representation == EVoxelTerrainRepresentation::VoxelProxy ? InSettings.Lod.MaximumVoxelProxyLevel
							  : Representation == EVoxelTerrainRepresentation::Surface	? InSettings.Lod.MaximumSurfaceSpatialLevel
																						: InSettings.Lod.MaximumMacroSpatialLevel;

		const int32 MaximumSamplesPerAxis = Representation == EVoxelTerrainRepresentation::VoxelProxy ? InSettings.Lod.MaximumVoxelProxySamplesPerAxis
																									  : InSettings.Lod.MaximumHeightfieldSamplesPerAxis;

		FVoxelTerrainStagePlan Plan;
		const FVoxelTerrainPageCostLimits Limits{uint64(InSettings.Lod.MaximumPageEstimatedTriangles),
			uint64(InSettings.Lod.MaximumPageEstimatedMeshMegabytes) * 1024 * 1024, InSettings.Lod.MaximumPageEstimatedBuildMilliseconds};
		if (InPersistent)
		{
			FDomainCompositionFloor Floor = InterestFloor;
			if (Resources && (!Floor.Add(SourceScratch, *Resources) || !Floor.Add(CompositionLease, *Resources)))
			{ OutError = TEXT("CapacityUnachievable: coarse caller floor"); return false; }
			CandidatePlanner.AdditionalRequiredInput = Floor.Bytes; CandidatePlanner.AdditionalRequiredWorking = Floor.WorkingBytes;
			CandidatePlanner.AdditionalRequiredCount = Floor.LeaseCount;
			if (!CandidatePlanner.ApplySourceDelta(Representation, Sources, OutError) ||
				!CandidatePlanner.AdvanceDirtyRoots(Representation, Maximum, MaximumSamplesPerAxis, InManifest.Settings.MinZ, InManifest.Settings.MaxZ,
					InSettings.Lod.TargetScreenErrorPixels, InManifest.BaseSampleHash, InManifest.RegistryHash, Plan, OutError, &CandidatePlanner.Predecessors[int32(Representation) - 2], Limits)) return false;
		}
		else if (!FVoxelTerrainStagePlanner::Build(Representation, Sources, Maximum, MaximumSamplesPerAxis, InManifest.Settings.MinZ, InManifest.Settings.MaxZ,
				InSettings.Lod.TargetScreenErrorPixels, InManifest.BaseSampleHash, InManifest.RegistryHash, Plan, OutError, &CandidatePlanner.Predecessors[int32(Representation) - 2],
				Limits))
		{
			return false;
		}

		const int32 StageIndex = int32(Representation) - 2;
		if (!InPersistent || CandidatePlanner.PrefixAppliedVersions[StageIndex] != CandidatePlanner.Stages[StageIndex].GetOutputVersion())
		{
			if (!AddCoverage(Representation, Plan)) return false;
			if (InPersistent) CandidatePlanner.PrefixAppliedVersions[StageIndex] = CandidatePlanner.Stages[StageIndex].GetOutputVersion();
		}
		if (!InPersistent && !ComposeStage(Representation, Plan, Sources)) return false;

	}

	if (InPersistent)
	{
		if (!ComposeFine()) return false;
		for (const EVoxelTerrainRepresentation Representation :
			{EVoxelTerrainRepresentation::VoxelProxy, EVoxelTerrainRepresentation::Surface, EVoxelTerrainRepresentation::Macro})
		{
			const int32 Stage = int32(Representation) - 2;
			FVoxelTerrainStagePlan Plan;
			Plan.Resources = Resources; Plan.Consumer = Consumer;
			FDomainCompositionFloor Floor = InterestFloor;
			if (Resources && (!Floor.Add(SourceScratch, *Resources) || !Floor.Add(CompositionLease, *Resources) || !BindGraphFloor(Floor)))
			{ OutError = TEXT("CapacityUnachievable: coarse final export retained input"); return false; }
			Plan.RetainedInputBytes = Floor.Bytes; Plan.RetainedWorkingBytes = Floor.WorkingBytes; Plan.RetainedLeaseCount = Floor.LeaseCount;
			if (!FVoxelTerrainStagePlanner::ExportCurrentPlan(CandidatePlanner.Stages[Stage], Plan, OutError, true) ||
				!ComposeStage(Representation, Plan, CandidatePlanner.Sources[Stage]->Values)) return false;
		}
	}

	// 旧正文全部退场以后，才允许释放其独立组合租约。
	const auto PreviousCompositionLease = InOutInterest.CoarseCompositionLease;
	InOutInterest.VoxelProxy = MoveTemp(VoxelProxy);
	InOutInterest.Surface = MoveTemp(Surface);
	InOutInterest.Macro = MoveTemp(Macro);
	InOutInterest.Geometry = MoveTemp(Geometry);
	InOutInterest.ResolutionFloorPages = MoveTemp(ResolutionFloor);
	InOutInterest.CoarseFailedRootCount = FailedRootCount;
	InOutInterest.CoarseCompositionLease = MoveTemp(CompositionLease);
	for (const auto& Update : SourceSideUpdates)
		InOutInterest.Sources.FindChecked(Update.Source).FirstSideCells[Update.Stage] = Update.SideCells;
	if (InPersistent)
	{
		CandidatePlanner.bCapacityBlocked = false;
		CandidatePlanner.AdditionalRequiredInput = CandidatePlanner.AdditionalRequiredWorking = 0;
		CandidatePlanner.AdditionalRequiredCount = 0;
		*InPersistent = MoveTemp(CandidatePlanner);
	}
	OutError.Reset();
	bComplete = true;
	return true;
}
