#include "Voxel/Rendering/VoxelViewManager.h"

#include "GameFramework/Actor.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

FVoxelGameThreadContinuation FVoxelViewManager::AcquireCoverageCapacitySteps(const uint64 InBytes,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& InOutLease, bool& OutComplete) const
{
	OutComplete = false;
	if (InBytes == 0)
	{
		OutComplete = true;
		co_return;
	}
	const auto Resources = Scheduler.GetTerrainResources();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, TaskOwnerId, TaskOwnerIncarnation,
		{}, FMath::Max<uint64>(1, AppliedInterestRevision));
	const auto State = Resources->Snapshot();
	if (InBytes > State.Limits.WorkingBytes)
	{
		UE_LOG(LogTemp, Error, TEXT("Oversized: local coverage working capacity bytes=%llu limit=%llu"), InBytes, State.Limits.WorkingBytes);
		co_return;
	}
	// 容量释放可能依赖本轮门禁挡住的发布或 Apply；不能在门禁内等待。
	OutComplete = InOutLease ? InOutLease->Bytes() >= InBytes || InOutLease->Resize(InBytes) :
		Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, InBytes, InOutLease);
	co_return;
}

FVoxelGameThreadContinuation FVoxelViewManager::CaptureCoverageCandidateSteps(
	TSet<FIntVector>& OutFine, TSet<FVoxelViewKey>& OutProxy, TSet<FVoxelSurfaceTileKey>& OutSurface, TSet<FVoxelMacroTileKey>& OutMacro,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease, bool& OutComplete) const
{
	OutComplete = false;
	const int32 QueryCount = bActiveCoverageFull ? 1 : ActiveDirtyCoverageRects.Num();
	for (int32 Query = 0; Query < QueryCount; ++Query)
	{
		FVoxelCoverageLocalSnapshot Snapshot;
		if (bActiveCoverageFull)
			co_await CoverageIndex.CaptureAllSteps(Snapshot, true, false);
		else
		{
			const FVoxelCoverageRect Rect = ActiveDirtyCoverageRects[Query];
			co_await CoverageIndex.CaptureSteps(
				FIntVector(Rect.Min.X - 1, Rect.Min.Y - 1, MIN_int32 + 1), FIntVector(Rect.Max.X + 1, Rect.Max.Y + 1, MAX_int32), Snapshot, true, false);
		}
		if (Snapshot.bCountOnly) co_return;
		CoverageIndexNodesVisited += Snapshot.VisitedIndexNodes;
		CoverageRecordsExamined += Snapshot.ExaminedRecords;
		CoverageRecordsCaptured += Snapshot.Records.Num();
		CoverageReadGuards += Snapshot.Guards.Num();
		int64 Counts[4] = {OutFine.Num(), OutProxy.Num(), OutSurface.Num(), OutMacro.Num()};
		for (const auto& Record : Snapshot.Records)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			const int32 Kind = int32(Record->Geometry.Key.Representation) - 1;
			if (Kind >= 0 && Kind < 4) ++Counts[Kind];
		}
		for (const int64 Count : Counts)
			if (Count > MAX_int32)
			{
				UE_LOG(LogTemp, Error, TEXT("Oversized: local coverage candidate count"));
				co_return;
			}
		const uint64 OldBytes = VoxelGenerationAllocation::AllocatedBytes(OutFine) + VoxelGenerationAllocation::AllocatedBytes(OutProxy) + VoxelGenerationAllocation::AllocatedBytes(OutSurface) + VoxelGenerationAllocation::AllocatedBytes(OutMacro);
		const uint64 NewBytes = FMath::Max<uint64>(VoxelGenerationAllocation::AllocatedBytes(OutFine), VoxelGenerationAllocation::Private::SetReserveBytes<FIntVector>(OutFine, int32(Counts[0]))) +
			FMath::Max<uint64>(VoxelGenerationAllocation::AllocatedBytes(OutProxy), VoxelGenerationAllocation::Private::SetReserveBytes<FVoxelViewKey>(OutProxy, int32(Counts[1]))) +
			FMath::Max<uint64>(VoxelGenerationAllocation::AllocatedBytes(OutSurface), VoxelGenerationAllocation::Private::SetReserveBytes<FVoxelSurfaceTileKey>(OutSurface, int32(Counts[2]))) +
			FMath::Max<uint64>(VoxelGenerationAllocation::AllocatedBytes(OutMacro), VoxelGenerationAllocation::Private::SetReserveBytes<FVoxelMacroTileKey>(OutMacro, int32(Counts[3])));
		bool bCapacity = false;
		co_await AcquireCoverageCapacitySteps(OldBytes + NewBytes, OutLease, bCapacity);
		if (!bCapacity) co_return;
		OutFine.Reserve(int32(Counts[0]));
		OutProxy.Reserve(int32(Counts[1]));
		OutSurface.Reserve(int32(Counts[2]));
		OutMacro.Reserve(int32(Counts[3]));
		const uint64 ActualBytes = VoxelGenerationAllocation::AllocatedBytes(OutFine) + VoxelGenerationAllocation::AllocatedBytes(OutProxy) +
			VoxelGenerationAllocation::AllocatedBytes(OutSurface) + VoxelGenerationAllocation::AllocatedBytes(OutMacro);
		check(ActualBytes <= NewBytes);
		if (OutLease) verify(OutLease->Resize(ActualBytes));
		for (const auto& Record : Snapshot.Records)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			const auto& Key = Record->Geometry.Key;
			switch (Key.Representation)
			{
			case EVoxelTerrainRepresentation::Fine: OutFine.Add(Key.Coordinate); break;
			case EVoxelTerrainRepresentation::VoxelProxy: OutProxy.Add({Key.Coordinate, Key.Level}); break;
			case EVoxelTerrainRepresentation::Surface: OutSurface.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level}); break;
			case EVoxelTerrainRepresentation::Macro: OutMacro.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level}); break;
			default: break;
			}
		}
	}
	OutComplete = true;
}

FVoxelGameThreadContinuation FVoxelViewManager::BuildCoverageSnapshotSteps(FCoveragePassSnapshot& OutSnapshot) const
{
	++CoverageSnapshotBuildCount;
	OutSnapshot = {};
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CandidateLease;
	bool bCaptured = false;
	TSet<FIntVector> FineCandidates;
	TSet<FVoxelViewKey> ProxyCandidates;
	TSet<FVoxelSurfaceTileKey> SurfaceCandidates;
	TSet<FVoxelMacroTileKey> MacroCandidates;
	co_await CaptureCoverageCandidateSteps(FineCandidates, ProxyCandidates, SurfaceCandidates, MacroCandidates, CandidateLease, bCaptured);
	if (!bCaptured) co_return;
	const int64 FineBranches = int64(FineCandidates.Num()) * 21;
	const int64 ProxyBranches = int64(ProxyCandidates.Num()) * 21;
	const int64 HeightfieldCount = int64(SurfaceCandidates.Num()) + MacroCandidates.Num();
	int64 ProxyRows = 0;
	for (const auto& Node : ProxyCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (const auto Data = ProxySurfaceCoverage.FindRef(Node)) ProxyRows += Data->TopSurfaceCoverage.Num();
	}
	if (FineBranches > MAX_int32 || ProxyBranches > MAX_int32 || HeightfieldCount > MAX_int32 || ProxyRows > MAX_int32)
	{
		UE_LOG(LogTemp, Error, TEXT("Oversized: local coverage snapshot count"));
		co_return;
	}
	const uint64 Bytes = VoxelGenerationAllocation::Private::SetReserveBytes<FIntVector>(OutSnapshot.PresentedFineSections, FineCandidates.Num()) +
		VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FIntVector, FVoxelFinePageKey>>(OutSnapshot.PresentedFineOwners, FineCandidates.Num()) +
		VoxelGenerationAllocation::Private::SetReserveBytes<FVoxelViewKey>(OutSnapshot.FineNodes, FineCandidates.Num()) +
		VoxelGenerationAllocation::Private::SetReserveBytes<FVoxelViewKey>(OutSnapshot.ProxyNodes, ProxyCandidates.Num()) +
		VoxelGenerationAllocation::Private::SetReserveBytes<FVoxelViewKey>(OutSnapshot.FineBranches, int32(FineBranches)) +
		VoxelGenerationAllocation::Private::SetReserveBytes<FVoxelViewKey>(OutSnapshot.ProxyBranches, int32(ProxyBranches)) +
		VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelCoverageBox, FDefaultAllocator>(FineCandidates.Num()) +
		VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelCoverageBox, FDefaultAllocator>(ProxyCandidates.Num()) +
		VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelCoverageRect, FDefaultAllocator>(int32(ProxyRows)) +
		VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelCoverageRect, FDefaultAllocator>(SurfaceCandidates.Num()) +
		VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelCoverageRect, FDefaultAllocator>(MacroCandidates.Num()) +
		2 * VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelCoverageRect, FDefaultAllocator>(int32(HeightfieldCount));
	bool bCapacity = false;
	co_await AcquireCoverageCapacitySteps(Bytes, OutSnapshot.ResourceLease, bCapacity);
	if (!bCapacity) co_return;
	OutSnapshot.PresentedFineSections.Reserve(FineCandidates.Num());
	OutSnapshot.PresentedFineOwners.Reserve(FineCandidates.Num());
	OutSnapshot.FineNodes.Reserve(FineCandidates.Num());
	OutSnapshot.ProxyNodes.Reserve(ProxyCandidates.Num());
	OutSnapshot.FineBranches.Reserve(int32(FineBranches));
	OutSnapshot.ProxyBranches.Reserve(int32(ProxyBranches));
	OutSnapshot.Fine.Reserve(FineCandidates.Num());
	OutSnapshot.Proxy.Reserve(ProxyCandidates.Num());
	OutSnapshot.ProxySurface.Reserve(int32(ProxyRows));
	OutSnapshot.Surface.Reserve(SurfaceCandidates.Num());
	OutSnapshot.Macro.Reserve(MacroCandidates.Num());
	OutSnapshot.Heightfield.Reserve(int32(HeightfieldCount));
	OutSnapshot.WantedHeightfield.Reserve(int32(HeightfieldCount));
	const uint64 ActualBytes = VoxelGenerationAllocation::AllocatedBytes(OutSnapshot.PresentedFineSections) +
		VoxelGenerationAllocation::AllocatedBytes(OutSnapshot.PresentedFineOwners) +
		VoxelGenerationAllocation::AllocatedBytes(OutSnapshot.FineNodes) +
		VoxelGenerationAllocation::AllocatedBytes(OutSnapshot.ProxyNodes) +
		VoxelGenerationAllocation::AllocatedBytes(OutSnapshot.FineBranches) +
		VoxelGenerationAllocation::AllocatedBytes(OutSnapshot.ProxyBranches) +
		VoxelGenerationAllocation::AllocatedBytes(OutSnapshot.Fine) +
		VoxelGenerationAllocation::AllocatedBytes(OutSnapshot.Proxy) +
		VoxelGenerationAllocation::AllocatedBytes(OutSnapshot.ProxySurface) +
		VoxelGenerationAllocation::AllocatedBytes(OutSnapshot.Surface) +
		VoxelGenerationAllocation::AllocatedBytes(OutSnapshot.Macro) +
		VoxelGenerationAllocation::AllocatedBytes(OutSnapshot.Heightfield) +
		VoxelGenerationAllocation::AllocatedBytes(OutSnapshot.WantedHeightfield);
	check(ActualBytes <= Bytes);
	if (OutSnapshot.ResourceLease) verify(OutSnapshot.ResourceLease->Resize(ActualBytes));
	for (const FIntVector& Section : FineCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (!GetInputRoot().FineWanted.Contains(Section) || !VisibleTerrainNodes.Contains({Section, 0}) || !IsFinePresented(Section)) continue;
		OutSnapshot.PresentedFineSections.Add(Section);
		if (const auto* Owner = PresentedFineSectionOwners.Find(Section)) OutSnapshot.PresentedFineOwners.Add(Section, *Owner);
		OutSnapshot.Fine.Add(FineCoverageBox(Section));
		OutSnapshot.FineNodes.Add({Section, 0});
	}
	for (const FVoxelViewKey& Node : ProxyCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (!VoxelProxyReady.Contains(Node) || !GetInputRoot().ProxyWanted.Contains(Node) || !VisibleTerrainNodes.Contains(Node)) continue;
		AActor* Actor = VoxelProxyActors.FindRef(Node);
		if (Actor && (Actor->IsHidden() || !Publisher->IsCommitted(Actor))) continue;
		OutSnapshot.ProxyNodes.Add(Node);
		OutSnapshot.Proxy.Add(VoxelProxyCoverageBox(Node));
		if (const auto Data = ProxySurfaceCoverage.FindRef(Node))
		{
			if (Data->TopSurfaceCoverage.Num() > ProxyRows - OutSnapshot.ProxySurface.Num()) co_return;
			co_await VoxelGameThread::AppendSteps(OutSnapshot.ProxySurface, Data->TopSurfaceCoverage);
		}
	}
	for (const auto& Key : SurfaceCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		AActor* Actor = SurfaceActors.FindRef(Key);
		if (GetInputRoot().SurfaceWanted.Contains(Key) && SurfaceReady.Contains(Key) && Actor && !Actor->IsHidden() && Publisher->IsCommitted(Actor))
			OutSnapshot.Surface.Add(SurfaceCoverageRect(Key));
	}
	for (const auto& Key : MacroCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		AActor* Actor = MacroActors.FindRef(Key);
		if (GetInputRoot().MacroWanted.Contains(Key) && MacroReady.Contains(Key) && Actor && !Actor->IsHidden() && Publisher->IsCommitted(Actor))
			OutSnapshot.Macro.Add(MacroCoverageRect(Key));
	}
	co_await VoxelGameThread::AppendSteps(OutSnapshot.Heightfield, OutSnapshot.Surface);
	co_await VoxelGameThread::AppendSteps(OutSnapshot.Heightfield, OutSnapshot.Macro);
	for (const auto& Key : SurfaceCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		OutSnapshot.WantedHeightfield.Add(SurfaceCoverageRect(Key));
	}
	for (const auto& Key : MacroCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		OutSnapshot.WantedHeightfield.Add(MacroCoverageRect(Key));
	}
	const auto BuildBranches = [](const TSet<FVoxelViewKey>& Nodes, TSet<FVoxelViewKey>& Branches) -> FVoxelGameThreadContinuation
	{
		for (FVoxelViewKey Node : Nodes)
		{
			while (Node.Level <= 20)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				if (Branches.Contains(Node)) break;
				Branches.Add(Node);
				Node = Node.GetParent();
			}
		}
	};
	co_await BuildBranches(OutSnapshot.FineNodes, OutSnapshot.FineBranches);
	co_await BuildBranches(OutSnapshot.ProxyNodes, OutSnapshot.ProxyBranches);
	OutSnapshot.bComplete = true;
}
