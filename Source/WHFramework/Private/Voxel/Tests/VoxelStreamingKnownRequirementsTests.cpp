#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"
#include "Voxel/Rendering/VoxelFinePage.h"
#include "Voxel/Rendering/VoxelCoverageIndex.h"
#include "Voxel/Rendering/VoxelCoveragePartition.h"
#include "Voxel/Rendering/VoxelPagePresentation.h"
#include "Voxel/Rendering/VoxelPublishBudget.h"
#include "Voxel/Rendering/VoxelBoundaryRaster.h"
#include "Voxel/Network/VoxelGeometryWire.h"
#include "Voxel/Streaming/VoxelInterestManager.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Generation/VoxelWorldManifest.h"
#include "Voxel/Task/VoxelTaskFairSelection.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelKnownInitialSizingTest,
	"WHFramework.Voxel.KnownRequirements.InitialSizing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelKnownInitialSizingTest::RunTest(const FString& InParameters)
{
	FString Error;
	uint8 Level = 0;
	TestTrue(TEXT("16-cell initial size"), VoxelStreamingGeometry::ValidateInitialSide(16, 3, Error));
	TestTrue(TEXT("32-cell initial size"), VoxelStreamingGeometry::ValidateInitialSide(32, 3, Error));
	TestFalse(TEXT("Non-dyadic size rejected"), VoxelStreamingGeometry::ValidateInitialSide(24, 3, Error));
	TestFalse(TEXT("Maximum-level conflict rejected"), VoxelStreamingGeometry::ValidateInitialSide(32, 0, Error));
	TestTrue(TEXT("Resolve stable level"), VoxelStreamingGeometry::TryLevel(32, Level, Error));
	TestEqual(TEXT("32 cells are absolute L1"), Level, uint8(1));
	TestEqual(TEXT("Negative floor division"), VoxelStreamingGeometry::FloorDivide(-1, 32), int64(-1));
	const int32 Replan = VoxelStreamingGeometry::ResolveReplanStep(80, 1.0, 32);
	TestEqual(TEXT("Radius cap does not turn the result into a non-power-of-two"), Replan, 64);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelKnownFineFamiliesTest,
	"WHFramework.Voxel.KnownRequirements.FineFamilies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelKnownFineFamiliesTest::RunTest(const FString& InParameters)
{
	FString Error;
	FVoxelViewSettings Settings;
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -256;
	Manifest.Settings.MaxZ = 256;
	TArray<FVoxelStreamingSource> Sources;
	FVoxelStreamingSource& Source = Sources.AddDefaulted_GetRef();
	Source.Id = FGuid::NewGuid();
	Source.Center = FIntVector(1);
	Source.bInheritWorldView = false;
	Source.View.InitialChunkSideCells = 32;
	Source.View.FineRadiusCells = 256;
	Source.View.FineVerticalHalfExtentCells = 256;
	Source.Capabilities = EVoxelStreamingCapability::Data | EVoxelStreamingCapability::FineVisual;
	FVoxelInterestSet Interest;
	const FVoxelInterestManager Manager;
	Interest = Manager.Compute(Sources, Manifest, Settings);
	TestTrue(TEXT("Independent domains plan"), Interest.PlanningError.IsEmpty());
	TestTrue(TEXT("Shard window has independent demand"), !Interest.FineShardDemands.IsEmpty());
	const int32 RuntimeSections = Interest.Exact.Num();
	const int32 Shards32 = Interest.FineShardDemands.Num();
	Settings.Fine.ShardSideCells = 64;
	const FVoxelInterestSet Shards64 = Manager.Compute(Sources, Manifest, Settings);
	TestTrue(TEXT("64-cell canonical shards plan"), Shards64.PlanningError.IsEmpty());
	TestEqual(TEXT("Shard size cannot expand Runtime demand"), Shards64.Exact.Num(), RuntimeSections);
	TestTrue(TEXT("Larger canonical shards reduce shard count"), Shards64.FineShardDemands.Num() < Shards32);
	for (const auto& Pair : Interest.FinePagePlan->SectionOwners)
	{
		TestEqual(TEXT("Only F0 owns Runtime Section meshes"), Pair.Value.Level, uint8(0));
	}
	for (const auto& Pair : Interest.FineShardOwners)
	{
		TestTrue(TEXT("Shard ownership is independent of Runtime Section ownership"), Pair.Value.Level > 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelKnownSamplingTest,
	"WHFramework.Voxel.KnownRequirements.SamplingIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelKnownSamplingTest::RunTest(const FString& InParameters)
{
	FString Error;
	int32 Step = 0;
	bool bResolutionFloor = false;
	TestTrue(TEXT("SSE calculation"), VoxelStreamingGeometry::ResolveSampleStep(1024, 90, 1080, 8, 4096, Step, bResolutionFloor, Error));
	TestEqual(TEXT("Actual sample step"), Step, 8);
	FVoxelTerrainGeometry Geometry;
	Geometry.Key = {EVoxelTerrainRepresentation::Fine, FIntVector::ZeroValue, 5};
	Geometry.SampleStepCells = 1;
	TArray<FVoxelTerrainBuildShard> Shards;
	TestTrue(TEXT("Coverage and sampling are independent"), VoxelStreamingGeometry::BuildShards(Geometry, 64, 4096, Shards, Error));
	TestEqual(TEXT("512-cell coverage at step 1 is sharded, not coarsened"), Shards.Num(), 512);
	TestEqual(TEXT("Sample step unchanged"), Geometry.SampleStepCells, 1);
	Geometry.Key.Representation = EVoxelTerrainRepresentation::VoxelProxy;
	TestFalse(TEXT("Proxy cannot claim exact sampling"), Geometry.Validate(Error));
	Geometry.SampleStepCells = 2;
	TestTrue(TEXT("Actual Proxy step is explicit"), Geometry.Validate(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelKnownEmptyPageTest,
	"WHFramework.Voxel.KnownRequirements.EmptyPageConvergence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelKnownEmptyPageTest::RunTest(const FString& InParameters)
{
	FVoxelPagePublicationInput Input;
	Input.Desired = {{1, 1, 1, 1}, true, true, true};
	Input.bCompleteFamily = true;
	Input.bAllMeshesReady = true;
	Input.bNewDependenciesReady = true;
	TestTrue(TEXT("Empty page still publishes ownership"), VoxelPagePresentation::Decide(Input) == EVoxelPagePublicationDecision::Publish);
	Input.Committed = Input.Desired;
	Input.bLayoutSame = true;
	int32 UnexpectedPublishes = 0;
	for (int32 Iteration = 0; Iteration < 10000; ++Iteration)
	{
		UnexpectedPublishes += VoxelPagePresentation::Decide(Input) != EVoxelPagePublicationDecision::NoChange;
	}
	TestEqual(TEXT("No actor does not cause a publish loop"), UnexpectedPublishes, 0);
	Input.Desired.bVisible = false;
	TestTrue(TEXT("Empty ownership needs a complete replacement"), VoxelPagePresentation::Decide(Input) == EVoxelPagePublicationDecision::KeepPrevious);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelKnownLocalIndexTest,
	"WHFramework.Voxel.KnownRequirements.LocalCoverageIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelKnownLocalIndexTest::RunTest(const FString& InParameters)
{
	FVoxelCoverageIndex Index;
	FString Error;
	auto MakeRecord = [](const FIntVector& Coordinate, const uint64 Revision)
	{
		auto Record = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
		Record->Geometry.Key = {EVoxelTerrainRepresentation::Fine, Coordinate, 0};
		Record->Stamp = {1, 1, Revision, 1};
		Record->Geometry.Key.TryBounds(0, 16, Record->Min, Record->Max);
		return Record;
	};
	TestTrue(TEXT("Near insert"), Index.Upsert(MakeRecord(FIntVector::ZeroValue, 1), Error));
	TestTrue(TEXT("Distant insert"), Index.Upsert(MakeRecord(FIntVector(10000, 10000, 0), 1), Error));
	FVoxelCoverageLocalSnapshot Snapshot;
	Index.Capture(FIntVector::ZeroValue, FIntVector(16), Snapshot);
	TestEqual(TEXT("Only one intersecting record"), Snapshot.Records.Num(), 1);
	TestTrue(TEXT("Distant update"), Index.Upsert(MakeRecord(FIntVector(10000, 10000, 0), 2), Error));
	TestTrue(TEXT("Unrelated update keeps local snapshot valid"), Index.IsCurrent(Snapshot));
	TestTrue(TEXT("Near update"), Index.Upsert(MakeRecord(FIntVector::ZeroValue, 2), Error));
	TestFalse(TEXT("Related update rejects stale snapshot"), Index.IsCurrent(Snapshot));
	FVoxelCoverageLocalSnapshot Inputs;
	Index.Capture(FIntVector::ZeroValue, FIntVector(16), Inputs, false);
	Index.Capture(FIntVector::ZeroValue, FIntVector(16), Snapshot);
	auto Presented = MakeRecord(FIntVector::ZeroValue, 2);
	Presented->bCommitted = true;
	TestTrue(TEXT("Presentation progresses without changing inputs"), Index.Upsert(Presented, Error));
	TestTrue(TEXT("Prepared input survives presentation progress"), Index.IsCurrent(Inputs));
	TestFalse(TEXT("Presentation readers still reject changed ownership"), Index.IsCurrent(Snapshot));
	TestTrue(TEXT("New geometry revision is accepted"), Index.Upsert(MakeRecord(FIntVector::ZeroValue, 3), Error));
	TestFalse(TEXT("Input readers still reject revised geometry"), Index.IsCurrent(Inputs));
	Index.Capture(FIntVector::ZeroValue, FIntVector(32), Inputs, false);
	TestTrue(TEXT("New local dependency is inserted"), Index.Upsert(MakeRecord(FIntVector(1, 0, 0), 1), Error));
	TestFalse(TEXT("Input readers detect newly inserted dependencies"), Index.IsCurrent(Inputs));
	Index.Capture(FIntVector::ZeroValue, FIntVector(32), Inputs, false);
	Index.Remove(Presented->Geometry.Key);
	TestFalse(TEXT("Input readers detect dependency removal"), Index.IsCurrent(Inputs));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelKnownCoverageBoundaryCountTest,
	"WHFramework.Voxel.KnownRequirements.CoverageBeyond64Edges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelKnownCoverageBoundaryCountTest::RunTest(const FString& InParameters)
{
	FString Error;
	TArray<FVoxelCoverageRegion> Required;
	TArray<FVoxelCoverageRegion> Cover;
	Required.Add({FIntVector::ZeroValue, FIntVector(96, 2, 2)});
	for (int32 Cell = 0; Cell < 96; ++Cell)
	{
		Cover.Add({FIntVector(Cell, 0, 0), FIntVector(Cell + 1, 2, 2)});
	}
	FVoxelCoverageEvaluationTask Task;
	TestTrue(TEXT("Begin coverage proof"), Task.Begin(Required, Cover, Error));
	for (int32 Slice = 0; Slice < 10000 && Task.GetState() == EVoxelCoverageEvaluation::Pending; ++Slice)
	{
		Task.Advance(4);
	}
	TestTrue(TEXT("More than 64 boundaries still cover"), Task.GetState() == EVoxelCoverageEvaluation::Covered);
	Cover.RemoveAt(47);
	TestTrue(TEXT("Begin missing-piece proof"), Task.Begin(Required, Cover, Error));
	for (int32 Slice = 0; Slice < 10000 && Task.GetState() == EVoxelCoverageEvaluation::Pending; ++Slice)
	{
		Task.Advance(4);
	}
	TestTrue(TEXT("One missing region is not discarded"), Task.GetState() == EVoxelCoverageEvaluation::Uncovered);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelKnownServiceFairnessTest,
	"WHFramework.Voxel.KnownRequirements.ServiceFairness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelKnownServiceFairnessTest::RunTest(const FString& InParameters)
{
	TArray<FVoxelTaskServiceCandidate> Candidates;
	constexpr uint8 Count = static_cast<uint8>(EVoxelTaskServiceLane::Count);
	int32 Served[Count] = {};
	for (uint8 Lane = 1; Lane < Count; ++Lane)
	{
		Candidates.Add({Lane, static_cast<EVoxelTaskServiceLane>(Lane), EVoxelTaskBlockReason::None, 0, 0.0, 0.0, Lane});
	}
	FVoxelTaskFairSelection Selection;
	for (int32 Dispatch = 0; Dispatch < 1000; ++Dispatch)
	{
		const int32 Chosen = Selection.Select(Candidates, 10.0);
		if (Chosen <= 0 || Chosen >= Count)
		{
			AddError(TEXT("Fair selector returned an invalid candidate"));
			return false;
		}
		++Served[Chosen];
	}
	TestEqual(TEXT("Critical retains its weighted preference"), Served[1], 800);
	for (uint8 Lane = 2; Lane < Count; ++Lane)
	{
		TestTrue(FString::Printf(TEXT("Service lane %u makes progress"), Lane), Served[Lane] >= 18);
	}
	Selection.Reset();
	Candidates = {
		{1, EVoxelTaskServiceLane::Critical, EVoxelTaskBlockReason::None, 0, 0.0, 0.0, 1, false},
		{2, EVoxelTaskServiceLane::Critical, EVoxelTaskBlockReason::None, 0, 100000.0, 9.9, 2, true}
	};
	for (int32 Dispatch = 0; Dispatch < 4; ++Dispatch)
		TestEqual(TEXT("Local handoff dependencies retain bounded priority over older ordinary Critical work"), Selection.Select(Candidates, 10.0), 2);
	TestEqual(TEXT("Continuous dependencies cannot starve ordinary Critical work"), Selection.Select(Candidates, 10.0), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelKnownGeometryWireTest,
	"WHFramework.Voxel.KnownRequirements.GeometryWire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelKnownGeometryWireTest::RunTest(const FString& InParameters)
{
	FString Error;
	FVoxelGeometryWireHeader Header;
	Header.Geometry.Key = {EVoxelTerrainRepresentation::Surface, FIntVector(-2, 3, 0), 5};
	Header.Geometry.SampleStepCells = 4;
	Header.Geometry.GeneratorIdentity = 42;
	Header.RequestId = 1;
	TArray<uint8> Bytes;
	TestTrue(TEXT("Encode descriptor"), VoxelGeometryWire::Encode(Header, Bytes, Error));
	TestEqual(TEXT("Fixed descriptor header size"), Bytes.Num(), 64);
	FVoxelGeometryWireHeader Decoded;
	TestTrue(TEXT("Decode descriptor"), VoxelGeometryWire::Decode(Bytes, Decoded, Error));
	TestTrue(TEXT("Round trip"), VoxelGeometryWire::ValidateExpected(Decoded, Header, Error));
	Decoded.Geometry.SampleStepCells = 8;
	TestFalse(TEXT("Wrong sampling is not the same result"), VoxelGeometryWire::ValidateExpected(Decoded, Header, Error));
	Bytes[4] = 5;
	TestFalse(TEXT("Old geometry protocol is rejected"), VoxelGeometryWire::Decode(Bytes, Decoded, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelKnownPublishBudgetTest,
	"WHFramework.Voxel.KnownRequirements.PublishBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelKnownPublishBudgetTest::RunTest(const FString& InParameters)
{
	FVoxelPublishBudget Budget;
	Budget.Begin(0.0, 5.0, 100);
	TestTrue(TEXT("Planning consumes the same frame budget"), Budget.TryStart(EVoxelPublishPhase::Plan, 0.0, 2.0));
	Budget.Finish(EVoxelPublishPhase::Plan, 0.0, 0.002);
	TestTrue(TEXT("Prepare uses the remaining budget"), Budget.TryStart(EVoxelPublishPhase::Prepare, 0.002, 2.0));
	Budget.Finish(EVoxelPublishPhase::Prepare, 0.002, 0.004);
	TestFalse(TEXT("Commit cannot claim a fresh budget"), Budget.TryStart(EVoxelPublishPhase::Commit, 0.004, 2.0));
	Budget.Begin(0.0, 5.0, 1);
	TestTrue(TEXT("The sole operation is admitted"), Budget.TryStart(EVoxelPublishPhase::Plan, 0.0, 0.0));
	Budget.Finish(EVoxelPublishPhase::Plan, 0.0, 0.001);
	TestEqual(TEXT("An exhausted operation quota cannot advertise time to another phase"), Budget.RemainingMilliseconds(0.001), 0.0);
	return true;
}

#endif
