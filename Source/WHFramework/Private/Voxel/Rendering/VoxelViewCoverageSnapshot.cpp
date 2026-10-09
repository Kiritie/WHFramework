#include "Voxel/Rendering/VoxelViewManager.h"

#include "GameFramework/Actor.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/Task/VoxelTerrainResources.h"

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
	uint64 LastReleaseVersion = MAX_uint64;
	while (true)
	{
		const auto State = Resources->Snapshot();
		if (InBytes > State.Limits.WorkingBytes)
		{
			UE_LOG(LogTemp, Error, TEXT("Oversized: local coverage working capacity bytes=%llu limit=%llu"), InBytes, State.Limits.WorkingBytes);
			co_return;
		}
		if (LastReleaseVersion != State.ReleaseVersion)
		{
			LastReleaseVersion = State.ReleaseVersion;
			if (InOutLease ? InOutLease->Bytes() >= InBytes || InOutLease->Resize(InBytes) :
				Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, InBytes, InOutLease))
			{
				OutComplete = true;
				co_return;
			}
		}
		co_await FVoxelGameThreadContinuation::Yield();
	}
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
			co_await CoverageIndex.CaptureAllSteps(Snapshot);
		else
		{
			const FVoxelCoverageRect Rect = ActiveDirtyCoverageRects[Query];
			co_await CoverageIndex.CaptureSteps(
				FIntVector(Rect.Min.X - 1, Rect.Min.Y - 1, MIN_int32 + 1), FIntVector(Rect.Max.X + 1, Rect.Max.Y + 1, MAX_int32), Snapshot);
		}
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
		const uint64 OldBytes = OutFine.GetAllocatedSize() + OutProxy.GetAllocatedSize() + OutSurface.GetAllocatedSize() + OutMacro.GetAllocatedSize();
		const uint64 NewBytes = FMath::Max<uint64>(OutFine.GetAllocatedSize(), VoxelBoundedAllocation::SetBytes<FIntVector>(int32(Counts[0]))) +
			FMath::Max<uint64>(OutProxy.GetAllocatedSize(), VoxelBoundedAllocation::SetBytes<FVoxelViewKey>(int32(Counts[1]))) +
			FMath::Max<uint64>(OutSurface.GetAllocatedSize(), VoxelBoundedAllocation::SetBytes<FVoxelSurfaceTileKey>(int32(Counts[2]))) +
			FMath::Max<uint64>(OutMacro.GetAllocatedSize(), VoxelBoundedAllocation::SetBytes<FVoxelMacroTileKey>(int32(Counts[3])));
		bool bCapacity = false;
		co_await AcquireCoverageCapacitySteps(OldBytes + NewBytes, OutLease, bCapacity);
		if (!bCapacity) co_return;
		OutFine.Reserve(int32(Counts[0]));
		OutProxy.Reserve(int32(Counts[1]));
		OutSurface.Reserve(int32(Counts[2]));
		OutMacro.Reserve(int32(Counts[3]));
		check(OutFine.GetAllocatedSize() + OutProxy.GetAllocatedSize() + OutSurface.GetAllocatedSize() + OutMacro.GetAllocatedSize() <= NewBytes);
		if (OutLease) verify(OutLease->Resize(NewBytes));
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
	const uint64 Bytes = VoxelBoundedAllocation::SetBytes<FIntVector>(FineCandidates.Num()) +
		VoxelBoundedAllocation::MapBytes<FIntVector, FVoxelFinePageKey>(FineCandidates.Num()) +
		VoxelBoundedAllocation::SetBytes<FVoxelViewKey>(FineCandidates.Num()) +
		VoxelBoundedAllocation::SetBytes<FVoxelViewKey>(ProxyCandidates.Num()) +
		VoxelBoundedAllocation::SetBytes<FVoxelViewKey>(int32(FineBranches)) +
		VoxelBoundedAllocation::SetBytes<FVoxelViewKey>(int32(ProxyBranches)) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelCoverageBox>(FineCandidates.Num()) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelCoverageBox>(ProxyCandidates.Num()) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelCoverageRect>(int32(ProxyRows)) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelCoverageRect>(SurfaceCandidates.Num()) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelCoverageRect>(MacroCandidates.Num()) +
		2 * VoxelBoundedAllocation::ArrayBytes<FVoxelCoverageRect>(int32(HeightfieldCount));
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
				bool bExists = false;
				Branches.Add(Node, &bExists);
				if (bExists) break;
				Node = Node.GetParent();
			}
		}
	};
	co_await BuildBranches(OutSnapshot.FineNodes, OutSnapshot.FineBranches);
	co_await BuildBranches(OutSnapshot.ProxyNodes, OutSnapshot.ProxyBranches);
	OutSnapshot.bComplete = true;
}
