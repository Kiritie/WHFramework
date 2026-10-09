#include "Misc/AutomationTest.h"
#include "Voxel/Rendering/VoxelFineHandoff.h"
#include "Voxel/Rendering/VoxelFineShardBuild.h"
#include "Voxel/Rendering/VoxelTerrainRenderer.h"
#include "Voxel/Task/VoxelTerrainResources.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFineHandoffCapacityTest,
	"WHFramework.Voxel.Rendering.FineHandoffCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFineHandoffCapacityTest::RunTest(const FString&)
{
	const auto Consumer = FVoxelGenerationConsumerKey::Section(71, FGuid::NewGuid(), 1, {}, 1);
	FVoxelTerrainResourceLimits Limits;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!TestTrue(TEXT("Create original geometry ledger"), FVoxelTerrainResources::Create(Limits, Consumer, Resources, Error))) return false;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Control, Preparation;
	if (!TestEqual(TEXT("Pregrant original renderer control"), Resources->TryAcquire(Consumer,
		EVoxelTerrainResourcePool::Input, 128, Control), EVoxelTerrainResourceAdmission::Granted) ||
		!TestEqual(TEXT("Pregrant original transaction arrays"), Resources->TryAcquire(Consumer,
			EVoxelTerrainResourcePool::Staged, 4096, Preparation), EVoxelTerrainResourceAdmission::Granted)) return false;
	FVoxelFineHandoff Handoff(71);
	if (!TestTrue(TEXT("Bind the original world and control lease"), Handoff.BindResources(Resources.ToSharedRef(), Consumer, Control))) return false;
	FVoxelFineOwnershipChange Change;
	Change.Section = FIntVector(1, -2, 3);
	Change.After.Owner = {EVoxelFineOwnerKind::F1Plus, FGuid::NewGuid(), 1};
	Change.After.Content.WorldEpoch = 71;
	Change.After.Content.GeneratorIdentity = 1;
	Change.After.Content.GeometryIdentity = FGuid::NewGuid();
	Change.After.Content.SampleStepCells = 1;
	Change.After.bKnownEmpty = Change.After.bGeometryReady = Change.After.bCanStage = true;
	Change.After.GeometryLease = MakeShared<FVoxelTaskCustomPayload, ESPMode::ThreadSafe>();
	const auto Changes = MakeArrayView(&Change, 1);
	FVoxelFineHandoffTransaction Output;
	Output.Id = FGuid::NewGuid();
	const FGuid PreviousOutput = Output.Id;
	TestFalse(TEXT("Unfunded transaction is rejected before locks"), Handoff.Prepare(Changes, 1, Output, Error));
	TestTrue(TEXT("Unfunded error is terminal"), Error.StartsWith(TEXT("InvalidRequest")));
	TestFalse(TEXT("Rejected transaction creates no lock"), Handoff.IsLocked(Change.Section));
	const auto Snapshot = Resources->Snapshot();
	const uint64 HandleBytes = Snapshot.LeaseHandleBytes / uint64(Snapshot.LeaseCount);
	const uint64 BlockerBytes = Snapshot.Limits.InputBytes - Snapshot.Used[int32(EVoxelTerrainResourcePool::Input)] - HandleBytes - 16;
	const auto ExternalConsumer = FVoxelGenerationConsumerKey::Section(71, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	if (!TestEqual(TEXT("External input pressure is real"), Resources->TryAcquire(ExternalConsumer,
		EVoxelTerrainResourcePool::Input, BlockerBytes, Blocker), EVoxelTerrainResourceAdmission::Granted)) return false;
	const uint64 ReleaseVersion = Resources->ReleaseVersionExcludingOwner(Consumer);
	TestFalse(TEXT("Directory capacity blocks the original funded transaction"), Handoff.Prepare(Changes, 1, Output, Error, Preparation));
	TestTrue(TEXT("Temporary directory pressure has a typed error"), Error.StartsWith(TEXT("CapacityBlocked")));
	TestEqual(TEXT("Rejected output identity is unchanged"), Output.Id, PreviousOutput);
	TestFalse(TEXT("Temporary rejection creates no lock"), Handoff.IsLocked(Change.Section));
	TestEqual(TEXT("Rejected pregrant creates no capacity wake"), Resources->ReleaseVersionExcludingOwner(Consumer), ReleaseVersion);
	Blocker.Reset();
	TestTrue(TEXT("Real external release wakes the original owner"), Resources->ReleaseVersionExcludingOwner(Consumer) > ReleaseVersion);
	if (!TestTrue(TEXT("Same frozen transaction succeeds after release"), Handoff.Prepare(Changes, 1, Output, Error, Preparation))) return false;
	TestTrue(TEXT("Only the accepted transaction owns the lock"), Handoff.IsLocked(Change.Section));
	TestNotNull(TEXT("Accepted record exists"), Handoff.FindTransaction(Output.Id));
	TOptional<FVoxelFineHandoffTransaction> Canceled;
	TestEqual(TEXT("Cancellation consumes the accepted transaction"), Handoff.RequestCancel(Output.Id, Canceled),
		EVoxelFineCancelDisposition::CanceledBeforeSeal);
	TestFalse(TEXT("Cancellation releases its exact lock"), Handoff.IsLocked(Change.Section));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFineRenderInputIdentityTest,
	"WHFramework.Voxel.Rendering.FineRenderInputIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFineRenderInputIdentityTest::RunTest(const FString&)
{
	auto Geometry = MakeShared<FVoxelFineShardGeometry, ESPMode::ThreadSafe>();
	auto Boundary = MakeShared<FVoxelFineBoundarySnapshot, ESPMode::ThreadSafe>();
	auto& Input = Geometry->Attempt.Input;
	Input.WorldEpoch = 7; Input.Incarnation = 11; Input.GeneratorIdentity = 13;
	Input.RegistryRevision = 17; Input.ShapeRevision = 19; Input.DataRevision = 23;
	Input.OverlayRevision = 29; Input.MeshRevision = 31;
	Geometry->GeometryIdentity = FGuid::NewGuid();
	Geometry->bCoverageValidated = true;
	Boundary->Input = Input;
	Geometry->Boundary = Boundary;
	FVoxelTerrainRenderAtom Atom;
	Atom.FineGeometry = Geometry;
	Atom.Boundaries.Add(Boundary);
	Atom.Evidence.GeometryLease = Geometry;
	Atom.Evidence.Owner = {EVoxelFineOwnerKind::F1Plus, FGuid::NewGuid(), Input.Incarnation};
	auto& Content = Atom.Evidence.Content;
	Content.WorldEpoch = Input.WorldEpoch; Content.GeneratorIdentity = Input.GeneratorIdentity;
	Content.RegistryRevision = Input.RegistryRevision; Content.ShapeRevision = Input.ShapeRevision;
	Content.DataRevision = Input.DataRevision; Content.OverlayRevision = Input.OverlayRevision;
	Content.MeshRevision = Input.MeshRevision; Content.GeometryIdentity = Geometry->GeometryIdentity;
	Content.SampleStepCells = 1;
	TestTrue(TEXT("Exact geometry and boundary retain the full frozen input"), Atom.HasMatchingFineInputs());
	for (auto Member : {&FVoxelFineContentIdentity::WorldEpoch, &FVoxelFineContentIdentity::GeneratorIdentity,
		&FVoxelFineContentIdentity::RegistryRevision, &FVoxelFineContentIdentity::ShapeRevision,
		&FVoxelFineContentIdentity::DataRevision, &FVoxelFineContentIdentity::OverlayRevision,
		&FVoxelFineContentIdentity::MeshRevision})
	{
		++(Content.*Member);
		TestFalse(TEXT("Each stale content field rejects publication"), Atom.HasMatchingFineInputs());
		--(Content.*Member);
	}
	++Boundary->Input.OverlayRevision;
	TestFalse(TEXT("Same geometry with a stale boundary is rejected"), Atom.HasMatchingFineInputs());
	--Boundary->Input.OverlayRevision;
	++Atom.Evidence.Owner.Incarnation;
	TestFalse(TEXT("Same coordinate with another incarnation is rejected"), Atom.HasMatchingFineInputs());
	--Atom.Evidence.Owner.Incarnation;
	Content.SampleStepCells = 2;
	TestFalse(TEXT("Fine publication cannot substitute proxy sampling"), Atom.HasMatchingFineInputs());
	Content.SampleStepCells = 1;
	Geometry->bCoverageValidated = false;
	TestFalse(TEXT("Incomplete coverage cannot publish as Fine"), Atom.HasMatchingFineInputs());
	return true;
}

#endif
