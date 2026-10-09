#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Runtime/VoxelChangeHierarchy.h"
#include "Voxel/Runtime/VoxelChangeIndex.h"
#include "Voxel/Runtime/VoxelOverlaySnapshot.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

namespace
{
	bool PublishTestSection(
		FVoxelWorldRuntime& InRuntime,
		const FIntVector& InSection,
		const uint64 InRevision,
		FString& OutError)
	{
		FVoxelSection* Section = InRuntime.FindOrAllocate(InSection, 1);
		if (!Section)
		{
			return false;
		}
		const FVoxelSectionStamp Stamp = Section->Stamp;
		TArray<FVoxelBlockState> Base;
		Base.Init(FVoxelBlockState(), VoxelBlock::Volume);
		return InRuntime.PublishBase(InSection, Stamp, MoveTemp(Base), OutError) &&
			InRuntime.PublishFinal(InSection, InRevision, {}, {}, OutError);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelNaturalPublicationRevisionTest,
	"WHFramework.Voxel.Runtime.NaturalPublicationRevision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNaturalPublicationRevisionTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldRuntime Runtime(1, true, VoxelTest::MakeRegistry(), VoxelTest::MakeGenerator());
	const FIntVector Origin = FIntVector::ZeroValue;
	const FVoxelViewKey Proxy{FIntVector::ZeroValue, 1};
	const FVoxelSurfaceTileKey Surface{FIntPoint::ZeroValue, 0};
	const FVoxelMacroTileKey Macro{FIntPoint::ZeroValue, 0};
	FString Error;
	if (!TestTrue(TEXT("First natural section publishes"), PublishTestSection(Runtime, Origin, 0, Error))) return false;
	const FVoxelChangeHierarchy& Hierarchy = Runtime.GetChangeHierarchy();
	TestEqual(TEXT("Natural residency preserves procedural Proxy revision"), Hierarchy.GetVoxelProxyRevision(Proxy), 0ull);
	TestEqual(TEXT("Natural residency preserves procedural Surface revision"), Hierarchy.GetSurfaceRevision(Surface), 0ull);
	TestEqual(TEXT("Natural residency preserves procedural Macro revision"), Hierarchy.GetMacroRevision(Macro), 0ull);
	FVoxelPreparedEdit Prepared;
	if (!TestTrue(TEXT("An actual change prepares"),
		Runtime.PrepareEdit({{Origin, FVoxelBlockState(), FVoxelBlockState(1, 0)}}, {}, Prepared, Error))) return false;
	FVoxelEditBatch Batch;
	if (!TestTrue(TEXT("An actual change commits"), Runtime.CommitPreparedEdit(MoveTemp(Prepared), Batch, Error))) return false;
	const uint64 ChangedRevision = Hierarchy.GetVoxelProxyRevision(Proxy);
	TestTrue(TEXT("An actual change invalidates every representation"), ChangedRevision > 0 &&
		Hierarchy.GetSurfaceRevision(Surface) > 0 && Hierarchy.GetMacroRevision(Macro) > 0);
	if (!TestTrue(TEXT("Neighbor natural section publishes"), PublishTestSection(Runtime, FIntVector(1, 0, 0), 0, Error))) return false;
	TestEqual(TEXT("Neighbor residency cannot cancel a prepared changed Proxy"), Hierarchy.GetVoxelProxyRevision(Proxy), ChangedRevision);
	FVoxelWorldRuntime Replica(1, false, VoxelTest::MakeRegistry(), VoxelTest::MakeGenerator());
	if (!PublishTestSection(Replica, Origin, 0, Error)) return false;
	const TMap<int32, FVoxelBlockState> Overlay{{0, FVoxelBlockState(1, 0)}};
	Replica.InvalidateRemoteSection(Origin);
	const uint64 RemoteRevision = Replica.GetChangeHierarchy().GetVoxelProxyRevision(Proxy);
	TestTrue(TEXT("Remote replacement applies a modification"), Replica.PublishFinal(Origin, 1, Overlay, {}, Error));
	TestEqual(TEXT("Remote materialization does not duplicate replica content invalidation"),
		Replica.GetChangeHierarchy().GetVoxelProxyRevision(Proxy), RemoteRevision);
	Replica.InvalidateRemoteSection(Origin);
	const uint64 RestoredRevision = Replica.GetChangeHierarchy().GetVoxelProxyRevision(Proxy);
	TestTrue(TEXT("Remote restoration publishes natural data"), Replica.PublishFinal(Origin, 2, {}, {}, Error));
	TestTrue(TEXT("Restoring modified remote data still invalidates coarse representations"),
		RestoredRevision > RemoteRevision);
	TestEqual(TEXT("Restoration materialization preserves the replica event version"),
		Replica.GetChangeHierarchy().GetVoxelProxyRevision(Proxy), RestoredRevision);
	TestFalse(TEXT("Remote restoration clears the modification index"), Replica.GetChangeIndex().IsModified(Origin));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelChangeHierarchyRevisionTest,
	"WHFramework.Voxel.Runtime.ChangeHierarchyRevision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelChangeHierarchyRevisionTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelChangeHierarchy Hierarchy;
	const FIntVector Section(-17, 3, -2);
	const uint64 First = Hierarchy.InvalidateSection(Section);
	Hierarchy.ReleaseSection(Section);
	const uint64 Second = Hierarchy.InvalidateSection(Section);
	TestTrue(TEXT("Revision never reuses after release"), Second > First);
	TestEqual(TEXT("Section revision is current"), Hierarchy.GetSectionRevision(Section), Second);
	TestEqual(TEXT("Voxel proxy parent invalidated"), Hierarchy.GetVoxelProxyRevision({ FIntVector(-9, 1, -1), 1 }), Second);
	const uint64 FarRevision = Hierarchy.InvalidateSection(FIntVector(100, 0, 0));
	TestEqual(TEXT("Macro revision uses the actual 2048-cell tile"),
		Hierarchy.GetMacroRevision({ FIntPoint(0, 0), 7 }), FarRevision);
	TestEqual(TEXT("Unrelated macro tile is unchanged"),
		Hierarchy.GetMacroRevision({ FIntPoint(3, 0), 7 }), uint64(0));
	Hierarchy.SetVoxelProxyRevision({ FIntVector(20, 20, 20), 1 }, 100);
	TestEqual(TEXT("Revision keys distinguish LOD levels"),
		Hierarchy.GetVoxelProxyRevision({ FIntVector(20, 20, 20), 2 }), uint64(0));
	const uint64 BoundaryRevision = Hierarchy.InvalidateSection(FIntVector(2, 0, 0));
	TestEqual(TEXT("Neighbour halo dependency is invalidated"),
		Hierarchy.GetVoxelProxyRevision({ FIntVector(0, 0, 0), 1 }), BoundaryRevision);
	Hierarchy.SetNaturalInfluence(6, 34);
	const FIntVector TreeSection(0, 0, 0);
	const uint64 TreeRevision = Hierarchy.InvalidateSection(TreeSection);
	TestEqual(TEXT("Tree trunk edits invalidate canopy in the upper proxy"),
		Hierarchy.GetVoxelProxyRevision({ FIntVector(0, 0, 1), 1 }), TreeRevision);
	TestTrue(TEXT("Ready canopy proxy is invalidated by a trunk edit"),
		Hierarchy.AffectsVoxelProxy({ FIntVector(0, 0, 1), 1 }, TreeSection));
	TestFalse(TEXT("Unrelated higher proxy stays intact"),
		Hierarchy.AffectsVoxelProxy({ FIntVector(0, 0, 2), 1 }, TreeSection));
	Hierarchy.SetNaturalInfluence(20, 34);
	const FIntVector NeighborTreeSection(-2, 0, 0);
	const uint64 NeighborRevision = Hierarchy.InvalidateSection(NeighborTreeSection);
	TestEqual(TEXT("Surface tile affected by a neighboring crown is invalidated"),
		Hierarchy.GetSurfaceRevision({ FIntPoint(0, 0), 0 }), NeighborRevision);
	TestTrue(TEXT("Surface dependency query includes neighboring crown"),
		Hierarchy.AffectsSurface({ FIntPoint(0, 0), 0 }, NeighborTreeSection));
	FVoxelChangeHierarchy Actual;
	const FVoxelSurfaceTileKey SurfaceKey{FIntPoint::ZeroValue, 5};
	FVoxelTerrainGeometry Descriptor{{EVoxelTerrainRepresentation::Surface, FIntVector::ZeroValue, 5}, 32, -128, 512, 17, 23};
	TestTrue(TEXT("Actual sampling registers exact dyadic index bounds"), Actual.RegisterGeometry(Descriptor));
	Descriptor.SampleStepCells = 1;
	TestTrue(TEXT("Sampling handoff registers the new descriptor"), Actual.RegisterGeometry(Descriptor));
	const uint64 RetainedRevision = Actual.InvalidateSection({-2, 0, 0});
	TestEqual(TEXT("Retained coarse sampling still invalidates its wider halo"), Actual.GetSurfaceRevision(SurfaceKey), RetainedRevision);
	const uint64 UnrelatedRevision = Actual.InvalidateSection({-10, 0, 0});
	TestEqual(TEXT("An unrelated edit does not invalidate every registered page"), Actual.GetSurfaceRevision(SurfaceKey), RetainedRevision);
	TestTrue(TEXT("Unrelated edit still advances its own section revision"), UnrelatedRevision > RetainedRevision);
	Actual.ReleaseGeometry(Descriptor.Key);
	Actual.InvalidateSection({0, 0, 0});
	TestEqual(TEXT("Retired geometry is removed from edit dependencies"), Actual.GetSurfaceRevision(SurfaceKey), RetainedRevision);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelChangeIndexNegativeCoordinateTest,
	"WHFramework.Voxel.Runtime.ChangeIndexNegativeCoordinate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelChangeIndexNegativeCoordinateTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelChangeIndex Index;
	const FIntVector Section(-1, -9, -2);
	Index.SetModified(Section, true);
	TestTrue(TEXT("Negative section is marked modified"), Index.IsModified(Section));
	TArray<FIntVector> Sections;
	Index.Enumerate({ Section * 16, (Section + FIntVector(1)) * 16 }, Sections);
	TestTrue(TEXT("Negative section enumerates from region mask"), Sections.Contains(Section));
	Index.SetModified(Section, false);
	TestFalse(TEXT("Restored section clears change bit"), Index.IsModified(Section));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelNaturalRestoreTest,
	"WHFramework.Voxel.Runtime.NaturalRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNaturalRestoreTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldRuntime Runtime(1, true, VoxelTest::MakeRegistry(), VoxelTest::MakeGenerator());
	const FIntVector Section = FIntVector::ZeroValue;
	const FIntVector Position = FIntVector::ZeroValue;
	FString Error;
	TestTrue(TEXT("Natural section publishes"), PublishTestSection(Runtime, Section, 0, Error));

	FVoxelPreparedEdit Prepared;
	TestTrue(
		TEXT("Modified edit prepares"),
		Runtime.PrepareEdit({ { Position, FVoxelBlockState(), FVoxelBlockState(1, 0) } }, {}, Prepared, Error));
	FVoxelEditBatch Batch;
	TestTrue(TEXT("Modified edit commits"), Runtime.CommitPreparedEdit(MoveTemp(Prepared), Batch, Error));
	const FVoxelSection* Modified = Runtime.FindSection(Section);
	TestNotNull(TEXT("Modified section remains resident"), Modified);
	if (Modified)
	{
		TestEqual(TEXT("Modified overlay has one cell"), Modified->Overlay.Num(), 1);
		TestTrue(TEXT("Modified section enters change index"), Runtime.GetChangeIndex().IsModified(Section));
	}

	TestTrue(
		TEXT("Natural restore prepares"),
		Runtime.PrepareEdit({ { Position, FVoxelBlockState(1, 0), FVoxelBlockState() } }, {}, Prepared, Error));
	TestTrue(TEXT("Natural restore commits"), Runtime.CommitPreparedEdit(MoveTemp(Prepared), Batch, Error));
	const FVoxelSection* Restored = Runtime.FindSection(Section);
	TestNotNull(TEXT("Restored section remains resident"), Restored);
	if (Restored)
	{
		TestTrue(TEXT("Natural restore removes overlay"), Restored->Overlay.IsEmpty());
		TestFalse(TEXT("Natural restore clears change index"), Runtime.GetChangeIndex().IsModified(Section));
		TestEqual(TEXT("Final cell equals natural base"), Restored->Blocks[0].Pack(), (*Restored->BaseBlocks)[0].Pack());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelPatchAtomicityTest,
	"WHFramework.Voxel.Runtime.PatchAtomicity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPatchAtomicityTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldRuntime Runtime(2, false, VoxelTest::MakeRegistry(), VoxelTest::MakeGenerator());
	const FIntVector SectionA(0, 0, 0);
	const FIntVector SectionB(1, 0, 0);
	FString Error;
	TestTrue(TEXT("First remote section publishes"), PublishTestSection(Runtime, SectionA, 5, Error));
	TestTrue(TEXT("Second remote section publishes"), PublishTestSection(Runtime, SectionB, 5, Error));

	FVoxelSectionPatch PatchA;
	PatchA.Section = SectionA;
	PatchA.FromRevision = 5;
	PatchA.ToRevision = 6;
	PatchA.Edits.Add({ 0, false, FVoxelBlockState(1, 0) });
	FVoxelSectionPatch PatchB = PatchA;
	PatchB.Section = SectionB;
	PatchB.FromRevision = 4;
	PatchB.ToRevision = 5;
	FVoxelEditBatch Batch;
	Batch.TransactionId = FGuid::NewGuid();
	Batch.Sections = { PatchA, PatchB };
	TestFalse(TEXT("Revision mismatch rejects the batch"), Runtime.ApplyRemotePatchBatch(Batch, Error));
	TestEqual(TEXT("First section revision remains atomic"), Runtime.FindSection(SectionA)->CommittedRevision, uint64(5));
	TestEqual(TEXT("Second section revision remains atomic"), Runtime.FindSection(SectionB)->CommittedRevision, uint64(5));
	TestTrue(TEXT("First section blocks remain unchanged"), Runtime.FindSection(SectionA)->Blocks[0].IsAir());
	TestTrue(TEXT("Second section blocks remain unchanged"), Runtime.FindSection(SectionB)->Blocks[0].IsAir());

	PatchB.FromRevision = 5;
	PatchB.ToRevision = 6;
	Batch.Sections = { PatchA, PatchB };
	TestTrue(TEXT("Matching revisions apply atomically"), Runtime.ApplyRemotePatchBatch(Batch, Error));
	TestEqual(TEXT("First section advances"), Runtime.FindSection(SectionA)->CommittedRevision, uint64(6));
	TestEqual(TEXT("Second section advances"), Runtime.FindSection(SectionB)->CommittedRevision, uint64(6));
	TestTrue(TEXT("Remote resync replaces an already ready section"), Runtime.PublishFinal(SectionA, 8, {}, {}, Error));
	TestEqual(TEXT("Resync advances committed revision"), Runtime.FindSection(SectionA)->CommittedRevision, uint64(8));
	TestTrue(TEXT("Resync restores the authoritative natural base"), Runtime.FindSection(SectionA)->Blocks[0].IsAir());
	TMap<int32, FVoxelBlockState> InvalidOverlay;
	InvalidOverlay.Add(VoxelBlock::Volume, FVoxelBlockState(1, 0));
	TestFalse(TEXT("Invalid resync cannot partially replace ready data"), Runtime.PublishFinal(SectionA, 9, InvalidOverlay, {}, Error));
	TestEqual(TEXT("Invalid resync leaves revision unchanged"), Runtime.FindSection(SectionA)->CommittedRevision, uint64(8));
	TestFalse(TEXT("Older resync cannot roll back committed data"), Runtime.PublishFinal(SectionA, 7, {}, {}, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelBulkHaloSnapshotTest,
	"WHFramework.Voxel.Runtime.BulkHaloSnapshot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelBulkHaloSnapshotTest::RunTest(const FString& Parameters)
{
	for (const FIntVector Center : { FIntVector(-2, -1, -3), FIntVector::ZeroValue, FIntVector(2, 1, 3) })
	{
		FVoxelWorldRuntime Runtime(1, true, VoxelTest::MakeRegistry(), VoxelTest::MakeGenerator());
		FString Error;
		if (!TestTrue(TEXT("Center publishes"), PublishTestSection(Runtime, Center, 0, Error))) return false;
		FVoxelSectionSnapshot Snapshot;
		TestTrue(TEXT("Snapshot without neighbors is valid"), Runtime.CaptureSnapshot(Center, Snapshot));
		const auto* OriginalBody = Snapshot.Blocks.GetData();
		const FVoxelSectionStamp OriginalStamp = Snapshot.Stamp;
		const uint64 OriginalRevision = Snapshot.Revision;
		for (uint8 Face = 0; Face < 6; ++Face) TestFalse(TEXT("Absent neighbor stays unknown"), Snapshot.Known[Face]);
		TestFalse(TEXT("Missing halo retains the original snapshot"), Runtime.CompleteSnapshotHalo(Snapshot, 0));
		for (uint8 Face = 0; Face < 6; ++Face)
		{
			const int32 Axis = Face / 2;
			FIntVector Key = Center;
			Key[Axis] += Face % 2 == 0 ? 1 : -1;
			FVoxelSection* Section = Runtime.FindOrAllocate(Key, 1);
			TArray<FVoxelBlockState> Blocks;
			for (int32 Index = 0; Index < 4096; ++Index) Blocks.Add(FVoxelBlockState(1 + (Index * 7 + Face) % 9, 0));
			TestTrue(TEXT("Neighbor base publishes"), Runtime.PublishBase(Key, Section->Stamp, MoveTemp(Blocks), Error));
			TestTrue(TEXT("Neighbor final publishes"), Runtime.PublishFinal(Key, 0, {}, {}, Error));
		}
		for (uint8 Face = 0; Face < 6; ++Face)
		{
			const auto* OriginalHalo = Snapshot.Halo[Face].GetData();
			TestTrue(TEXT("Missing halo completes in the frozen original snapshot"), Runtime.CompleteSnapshotHalo(Snapshot, Face));
			TestTrue(TEXT("Halo completion does not allocate another body"), Snapshot.Blocks.GetData() == OriginalBody);
			TestTrue(TEXT("Halo completion uses the prepaid face backing"), Snapshot.Halo[Face].GetData() == OriginalHalo);
			TestTrue(TEXT("Halo completion preserves the original instance"), Snapshot.Stamp == OriginalStamp);
			TestEqual(TEXT("Halo completion preserves the original revision"), Snapshot.Revision, OriginalRevision);
			TestTrue(TEXT("Loaded neighbor is known"), Snapshot.Known[Face]);
			const int32 Axis = Face / 2;
			for (int32 V = 0; V < 16; ++V)
			{
				for (int32 U = 0; U < 16; ++U)
				{
					FIntVector Position = Center * 16;
					Position[Axis] += Face % 2 == 0 ? 16 : -1;
					Position[(Axis + 1) % 3] += U;
					Position[(Axis + 2) % 3] += V;
					FVoxelBlockState Expected;
					TestTrue(TEXT("Reference cell exists"), Runtime.TryGetBlock(Position, Expected));
					TestEqual(TEXT("Bulk halo matches world sampling in every direction"), Snapshot.Halo[Face][U + 16 * V], Expected.Pack());
				}
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelOverlayBorrowedVisitorTest,
	"WHFramework.Voxel.Runtime.OverlayBorrowedVisitor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelOverlayBorrowedVisitorTest::RunTest(const FString& Parameters)
{
	FVoxelOverlaySnapshotSet Source;
	const FIntVector OwnedKey(-1, 0, 0);
	const FIntVector PinnedKey(0, 0, 0);
	FVoxelOverlaySnapshot& Owned = Source.Sections.Add(OwnedKey);
	Owned.Section = OwnedKey;
	Owned.Blocks.Add(3, FVoxelBlockState(1, 0));
	TSharedRef<FVoxelOverlaySnapshot, ESPMode::ThreadSafe> Pinned = MakeShared<FVoxelOverlaySnapshot, ESPMode::ThreadSafe>();
	Pinned->Section = PinnedKey;
	Pinned->Blocks.Add(7, FVoxelBlockState(2, 0));
	Source.PinnedSections.Add(PinnedKey, Pinned);
	FString Error;
	int32 Visited = 0;
	TestTrue(TEXT("Visitor borrows both frozen storage kinds"), Source.VisitModifiedOverlays(
		{FIntVector(-16, 0, 0), FIntVector(16)}, [&](const FIntVector& Key, const FVoxelOverlaySnapshot& Snapshot)
		{
			++Visited;
			TestTrue(TEXT("Visitor retains the original snapshot body"), &Snapshot == Source.Find(Key));
			const int32 Index = Key == OwnedKey ? 3 : 7;
			TestTrue(TEXT("Visitor retains the original block storage"), Snapshot.Blocks.Find(Index) == Source.Find(Key)->Blocks.Find(Index));
			return true;
		}, Error));
	TestEqual(TEXT("Each frozen section is visited once"), Visited, 2);
	Visited = 0;
	TestTrue(TEXT("Half-open bounds exclude the neighboring section"), Source.VisitModifiedOverlays(
		{FIntVector(-16, 0, 0), FIntVector(0, 16, 16)}, [&](const FIntVector& Key, const FVoxelOverlaySnapshot& Snapshot)
		{
			++Visited;
			TestTrue(TEXT("Only the negative section intersects"), Key == OwnedKey);
			return true;
		}, Error));
	TestEqual(TEXT("The section touching the maximum plane is excluded"), Visited, 1);
	TAtomic<bool> Cancel(false);
	TestFalse(TEXT("Cancellation from the final callback is retained"), Source.VisitModifiedOverlays(
		{FIntVector(-16, 0, 0), FIntVector(0, 16, 16)}, [&](const FIntVector&, const FVoxelOverlaySnapshot&)
		{
			Cancel.Store(true);
			return true;
		}, Error, &Cancel));
	TestEqual(TEXT("Cancellation remains explicit"), Error, FString(TEXT("Canceled")));
	TestFalse(TEXT("Consumer refusal keeps its typed error"), Source.VisitModifiedOverlays(
		{FIntVector(-16, 0, 0), FIntVector(0, 16, 16)}, [&](const FIntVector&, const FVoxelOverlaySnapshot&)
		{
			Error = TEXT("CapacityBlocked");
			return false;
		}, Error));
	TestEqual(TEXT("Borrowed visitor does not replace the admission error"), Error, FString(TEXT("CapacityBlocked")));
	FVoxelOverlaySnapshotSet Invalid;
	Invalid.Sections.Add(FIntVector(MAX_int32, 0, 0));
	TestFalse(TEXT("Section to cell overflow is rejected before visiting"), Invalid.VisitModifiedOverlays(
		{FIntVector::ZeroValue, FIntVector(16)}, [&](const FIntVector&, const FVoxelOverlaySnapshot&)
		{
			AddError(TEXT("Overflowing storage reached the consumer"));
			return true;
		}, Error));
	return true;
}

#endif
