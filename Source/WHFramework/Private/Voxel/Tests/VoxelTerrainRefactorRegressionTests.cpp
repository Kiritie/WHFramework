#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Voxel/Rendering/VoxelCoverageIndex.h"
#include "Voxel/Rendering/VoxelCoverage.h"
#include "Voxel/Rendering/VoxelPublishBudget.h"
#include "Voxel/Rendering/VoxelTerrainStagePlanner.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Rendering/VoxelViewManager.h"
#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/Streaming/VoxelEmergeManager.h"
#include "Voxel/Save/VoxelRegionStore.h"
#include "Voxel/Streaming/VoxelInterestManager.h"
#include "Voxel/Streaming/VoxelInterestRuntime.h"
#include "Voxel/VoxelModule.h"
#include "VoxelTestUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelInterestFineLayoutRevisionTest, "WHFramework.Voxel.Streaming.FineLayoutRevision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelInterestFineLayoutRevisionTest::RunTest(const FString& InParameters)
{
	(void)InParameters;

	FVoxelInterestRuntime Runtime;
	FVoxelWorldManifest Manifest;
	FVoxelViewSettings Settings;
	Settings.Lod.MaximumFineLevel = 2;

	FVoxelStreamingSource Source;
	Source.Id = FGuid::NewGuid();
	Source.Capabilities = EVoxelStreamingCapability::Data | EVoxelStreamingCapability::FineVisual | EVoxelStreamingCapability::WorldVisual;
	Source.View.FineRadiusCells = 512;
	Source.View.FineLevel0RadiusCells = 32;
	Source.View.FineVerticalHalfExtentCells = 128;
	Source.Center = FIntVector(8, 8, 8);
	Source.SchedulingPriority = 0;

	FVoxelInterestSet First;
	FVoxelInterestDelta Delta;
	bool bSnapshot = false;
	TestTrue(TEXT("Initial interest changes"), Runtime.Update(MakeArrayView(&Source, 1), Manifest, Settings, First, Delta, bSnapshot));
	if (!TestTrue(TEXT("Initial snapshot exists"), bSnapshot) || !TestTrue(TEXT("Initial Fine set exists"), First.FineSections.IsValid()) ||
		!TestTrue(TEXT("Initial Fine page plan exists"), First.FinePagePlan.IsValid()))
	{
		return false;
	}

	const TSet<FIntVector> FirstFine = *First.FineSections;
	const TSet<FVoxelFinePageKey> FirstPages = First.FinePagePlan->Pages;

	// 只改变 Page 最大合并级，不改变 Source 中心、FineRadius、ExactRadius、
	// CollisionRadius 或任何需求范围。旧实现会得到新的 FinePagePlan，
	// 但因为 logical demand/geometry/priorities 都相同而返回 bChanged=false。
	Settings.Lod.MaximumFineLevel = 3;

	FVoxelInterestSet Second;
	Delta.Reset();
	bSnapshot = false;
	const bool bChanged = Runtime.Update(MakeArrayView(&Source, 1), Manifest, Settings, Second, Delta, bSnapshot);

	if (!TestTrue(TEXT("Layout-only policy update produces accepted change"), bChanged && bSnapshot) ||
		!TestTrue(TEXT("Updated Fine set exists"), Second.FineSections.IsValid()) ||
		!TestTrue(TEXT("Updated Fine page plan exists"), Second.FinePagePlan.IsValid()))
	{
		return false;
	}

	TestTrue(TEXT("Fine logical demand is unchanged"),
		Second.FineSections->Num() == FirstFine.Num() && Second.FineSections->Includes(FirstFine) && FirstFine.Includes(*Second.FineSections));
	TestTrue(TEXT("Visual partition marks layout-only change"), Delta.bVisualPartitionChanged);
	TestFalse(TEXT("Fine page ownership is not silently reused"), Second.FinePagePlan->Pages.Num() == FirstPages.Num() &&
																	  Second.FinePagePlan->Pages.Includes(FirstPages) &&
																	  FirstPages.Includes(Second.FinePagePlan->Pages));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFinePageObsoleteDirtyCleanupTest, "WHFramework.Voxel.Rendering.FinePageObsoleteDirtyCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFinePageObsoleteDirtyCleanupTest::RunTest(const FString& InParameters)
{
	(void)InParameters;

	UVoxelModule* Module = NewObject<UVoxelModule>();
	FVoxelTaskScheduler Scheduler;
	FVoxelViewManager Manager(*Module, Scheduler, 1);

	const FVoxelFinePageKey Obsolete{FIntVector(5, 5, 0), 2};
	Manager.DirtyFinePages.Add(Obsolete);
	Manager.PreparedFinePageMeshes.Add(Obsolete, {17, MakeShared<FVoxelSectionMeshResult, ESPMode::ThreadSafe>()});
	Manager.PendingFinePageBuilds.Add(Obsolete, {});

	Manager.FinePagePlan.Pages.Reset();
	Manager.RetireFinePageBuildState(Obsolete, false);

	TestFalse(TEXT("Obsolete page leaves dirty workset"), Manager.DirtyFinePages.Contains(Obsolete));
	TestFalse(TEXT("Obsolete page releases prepared mesh"), Manager.PreparedFinePageMeshes.Contains(Obsolete));
	TestFalse(TEXT("Obsolete page releases pending signature"), Manager.PendingFinePageBuilds.Contains(Obsolete));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainSampleWorkBoundTest, "WHFramework.Voxel.Streaming.TerrainSampleWorkBound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainSampleWorkBoundTest::RunTest(const FString& InParameters)
{
	(void)InParameters;

	FVoxelStreamingSource Source;
	Source.Id = FGuid::NewGuid();
	Source.SchedulingPriority = 0;
	Source.Center = FIntVector::ZeroValue;
	Source.VerticalFovDegrees = 60.0f;
	Source.ViewportHeightPixels = 1440;

	FVoxelTerrainStageSource StageSource{Source, 2048, 256, 0};

	FVoxelTerrainStagePlan Plan;
	FString Error;
	TestTrue(TEXT("Proxy plan builds"), FVoxelTerrainStagePlanner::Build(EVoxelTerrainRepresentation::VoxelProxy, MakeArrayView(&StageSource, 1), 7, 64, -512,
											512, 8.0, 1, 1, Plan, Error, nullptr));

	FVoxelCoverageIndex FineCoverage;
	auto Fine = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
	Fine->Geometry = {{EVoxelTerrainRepresentation::Fine, FIntVector::ZeroValue, 0}, 1, -512, 512, 1, 1};
	Fine->Geometry.Key.TryBounds(-512, 512, Fine->Min, Fine->Max);
	TestTrue(TEXT("Fine fixture is indexed"), FineCoverage.Upsert(Fine, Error));
	TestTrue(TEXT("Proxy plan uses a concrete Fine predecessor"), FVoxelTerrainStagePlanner::Build(EVoxelTerrainRepresentation::VoxelProxy,
																	  MakeArrayView(&StageSource, 1), 7, 64, -512, 512, 8.0, 1, 1, Plan, Error, &FineCoverage));
	bool bHasStepOneOutsideFine = false;
	for (const auto& Pair : Plan.Pages)
	{
		const FVoxelTerrainGeometry& Geometry = Pair.Value;
		TestTrue(TEXT("Every Proxy page obeys max samples per axis"), Geometry.Key.SideCells() / Geometry.SampleStepCells <= 64);
		FIntVector Min, Max;
		Geometry.Key.TryBounds(-512, 512, Min, Max);
		const bool bOutsideFine =
			Max.X <= Fine->Min.X || Min.X >= Fine->Max.X || Max.Y <= Fine->Min.Y || Min.Y >= Fine->Max.Y || Max.Z <= Fine->Min.Z || Min.Z >= Fine->Max.Z;
		bHasStepOneOutsideFine |= Geometry.SampleStepCells == 1 && bOutsideFine;
		TestTrue(TEXT("Proxy identity never aliases Fine L0"), Geometry.Key.Level >= 1);
	}

	TestTrue(TEXT("SSE is allowed to retain 25cm outside Fine"), bHasStepOneOutsideFine);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFinePageAttemptIsolationTest, "WHFramework.Voxel.Rendering.FinePageAttemptIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelFinePageAttemptIsolationTest::RunTest(const FString& InParameters)
{
	UVoxelModule* Module = NewObject<UVoxelModule>();
	FVoxelTaskScheduler Scheduler;
	FVoxelViewManager Manager(*Module, Scheduler, 11);
	const FVoxelFinePageKey Page{FIntVector::ZeroValue, 1};
	const FVoxelFinePageKey Other{FIntVector(8, 0, 0), 1};
	Manager.FinePagePlan.Pages.Add(Page);
	Manager.FinePagePlan.Pages.Add(Other);
	Manager.InvalidateFinePageLayout(Page);
	Manager.InvalidateFinePageLayout(Other);
	const uint64 Version = Manager.FinePageVersions.FindRef(Page);
	const uint64 Content = Manager.FinePageContentRevisions.FindRef(Page);
	FVoxelTaskStamp Old;
	Old.WorldEpoch = 11;
	Old.Token = 1;
	Old.ViewKey = {Page.Coordinate, Page.Level};
	FVoxelTaskStamp Current = Old;
	Current.Token = 2;
	Manager.PendingFinePageBuilds.Add(Page, {Current, 73, Version, Content});
	Manager.InvalidateFinePageLayout(Other);
	TestEqual(TEXT("Unrelated page layout preserves this page version"), Manager.FinePageVersions.FindRef(Page), Version);
	FVoxelTaskResult Late;
	Late.Stamp = Old;
	Late.bCanceled = true;
	Manager.ApplyFinePageBuild(Page, 73, Version, Content, Old, MoveTemp(Late));
	TestTrue(TEXT("Same-signature late callback cannot clear the new attempt"), Manager.PendingFinePageBuilds.Contains(Page));
	TestEqual(TEXT("The new attempt still owns the page"), Manager.PendingFinePageBuilds.FindChecked(Page).Stamp.Token, uint64(2));
	FVoxelTaskResult Cancelled;
	Cancelled.Stamp = Current;
	Cancelled.bCanceled = true;
	Manager.ApplyFinePageBuild(Page, 73, Version, Content, Current, MoveTemp(Cancelled));
	TestFalse(TEXT("Matching cancellation releases its attempt"), Manager.PendingFinePageBuilds.Contains(Page));
	TestTrue(TEXT("Matching cancellation retains retry demand"), Manager.DirtyFinePages.Contains(Page));
	Current.Token = 3;
	Manager.PendingFinePageBuilds.Add(Page, {Current, 73, Version, Content});
	FVoxelTaskResult Failed;
	Failed.Stamp = Current;
	Failed.Error = TEXT("Rejected mesh fixture");
	AddExpectedError(TEXT("Fine page build failed"), EAutomationExpectedErrorFlags::Contains, 1);
	Manager.ApplyFinePageBuild(Page, 73, Version, Content, Current, MoveTemp(Failed));
	TestTrue(TEXT("A terminal failure is retained as an explicit page result"), Manager.FinePageFailures.Contains(Page));
	TestFalse(TEXT("An unchanged failed input cannot spin in the dirty queue"), Manager.DirtyFinePages.Contains(Page));
	TestEqual(TEXT("Failure remains available to acceptance diagnostics"), Manager.GetTerrainWorkMetrics().Failed, uint64(1));
	Manager.MarkFinePageDirty(Page);
	TestFalse(TEXT("A real content change can recover from the previous failure"), Manager.FinePageFailures.Contains(Page));
	Current.Token = 4;
	Manager.PendingFinePageBuilds.Add(Page, {Current, 73, Version, Manager.FinePageContentRevisions.FindRef(Page)});
	Manager.RetireFinePageBuildState(Page, true);
	TestFalse(TEXT("Retirement releases dirty work"), Manager.DirtyFinePages.Contains(Page));
	TestFalse(TEXT("Retirement releases attempt ownership"), Manager.PendingFinePageBuilds.Contains(Page));
	TestEqual(TEXT("Late and canceled results are distinguished"), Manager.GetTerrainWorkMetrics().Stale, uint64(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelSourceAcceptanceBarrierTest, "WHFramework.Voxel.Streaming.SourceAcceptanceBarrier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelSourceAcceptanceBarrierTest::RunTest(const FString& InParameters)
{
	UVoxelModule* Module = NewObject<UVoxelModule>();
	Module->Epoch = 11;
	FVoxelStreamingSource Source;
	Source.Id = FGuid::NewGuid();
	Source.Center = FIntVector(1, 2, 3);
	Source.Normalize();
	auto& Registered = Module->Sources.FindOrAdd(Source.Id);
	Registered.Owner = Module;
	Registered.Value = Source;
	Registered.PlannedValue = Source;
	FVoxelInterestSet Accepted;
	Accepted.Sources.FindOrAdd(Source.Id).Source = Source;
	Module->CurrentInterest = MakeShared<const FVoxelInterestSet, ESPMode::ThreadSafe>(Accepted);
	TestTrue(TEXT("Accepted source center passes"), Module->IsSourceAdmittedAt(Source.Id, Source.Center, 11));
	const FIntVector Moved = Source.Center + FIntVector(1, 0, 0);
	FVoxelStreamingSource MovedSource = Source;
	MovedSource.Center = Moved;
	Module->bInterestDirty = false;
	TestTrue(TEXT("Subcell relocation enters through the public source API"), Module->UpdateSource(Source.Id, MovedSource));
	TestTrue(TEXT("Subcell relocation requests an exact source acknowledgment"), Module->bInterestDirty);
	TestFalse(TEXT("Same planning cell cannot release a new spawn candidate early"), Module->IsSourceAdmittedAt(Source.Id, Moved, 11));
	Accepted.Sources.FindChecked(Source.Id).Source.Center = Moved;
	Module->CurrentInterest = MakeShared<const FVoxelInterestSet, ESPMode::ThreadSafe>(Accepted);
	TestTrue(TEXT("New complete source snapshot acknowledges the candidate"), Module->IsSourceAdmittedAt(Source.Id, Moved, 11));
	TestFalse(TEXT("World epoch invalidates source acknowledgment"), Module->IsSourceAdmittedAt(Source.Id, Moved, 10));
	Module->Sources.Remove(Source.Id);
	TestFalse(TEXT("Removed source cannot acknowledge a candidate"), Module->IsSourceAdmittedAt(Source.Id, Moved, 11));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCriticalDependencyAdmissionTest, "WHFramework.Voxel.Streaming.CriticalDependencyAdmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelCriticalDependencyAdmissionTest::RunTest(const FString& InParameters)
{
	FVoxelStreamingSource Source;
	Source.Id = FGuid::NewGuid();
	Source.Center = FIntVector(0, 0, 242);
	Source.Capabilities = EVoxelStreamingCapability::Data | EVoxelStreamingCapability::FineVisual | EVoxelStreamingCapability::Readiness;
	Source.bInheritWorldView = false;
	Source.View.FineRadiusCells = 96;
	Source.View.FineLevel0RadiusCells = 32;
	Source.View.FineVerticalHalfExtentCells = 64;
	FVoxelWorldManifest Manifest;
	FVoxelViewSettings Settings;
	const FVoxelInterestSet Planned = FVoxelInterestManager().Compute(MakeArrayView(&Source, 1), Manifest, Settings);
	if (!TestTrue(TEXT("Readiness fixture has playable Fine ownership"), Planned.PlayableFineKeys.Num() > 0 && Planned.FinePagePlan.IsValid()))
	{
		return false;
	}
	bool bHasVerticalBoundary = false;
	for (const FIntVector& Key : Planned.PlayableFineKeys)
	{
		const FVoxelExactDemand& Required = Planned.Exact.FindChecked(Key);
		TestTrue(
			TEXT("Every playable Fine Section receives critical mesh and data admission"), Required.bMovementCriticalFine && Required.bMovementCriticalData);
		const auto* Owner = Planned.FinePagePlan->SectionOwners.Find(Key);
		TestTrue(TEXT("Every playable Fine Section donates to its Page"), Owner && Planned.CriticalFinePages.Contains(*Owner));
		bHasVerticalBoundary |= FMath::Abs(Key.Z * 16 + 8 - Source.Center.Z) > Source.View.FineLevel0RadiusCells;
		for (const FIntVector Direction :
			{FIntVector(1, 0, 0), FIntVector(-1, 0, 0), FIntVector(0, 1, 0), FIntVector(0, -1, 0), FIntVector(0, 0, 1), FIntVector(0, 0, -1)})
		{
			if (const auto* Neighbor = Planned.Exact.Find(Key + Direction))
			{
				TestTrue(TEXT("Playable Fine halo retains critical data donation"), Neighbor->bMovementCriticalData);
			}
		}
	}
	TestTrue(TEXT("Fixture covers the cylinder boundary beyond the movement sphere"), bHasVerticalBoundary);
	UVoxelModule* Module = NewObject<UVoxelModule>();
	Module->Runtime = MakeUnique<FVoxelWorldRuntime>(1024, false, VoxelTest::MakeRegistry(), VoxelTest::MakeGenerator());
	FVoxelTaskScheduler Scheduler;
	FVoxelViewManager Manager(*Module, Scheduler, 11);
	FVoxelInterestSet Interest;
	const FVoxelTerrainSpatialKey Dependency{EVoxelTerrainRepresentation::VoxelProxy, FIntVector::ZeroValue, 1};
	Interest.CriticalDependencies.Add(Dependency);
	FVoxelExactDemand Demand;
	Demand.bMovementCriticalFine = true;
	Interest.Exact.Add(FIntVector::ZeroValue, Demand);
	Module->CurrentInterest = MakeShared<const FVoxelInterestSet, ESPMode::ThreadSafe>(Interest);
	TestTrue(TEXT("Fine movement participates in the critical chain"), Manager.HasCriticalNearFieldPending());
	TestTrue(TEXT("Proxy predecessor is explicitly admitted"), Manager.IsCriticalDependency(Dependency));
	TestFalse(TEXT("Unrelated Proxy expansion does not inherit Critical"),
		Manager.IsCriticalDependency({EVoxelTerrainRepresentation::VoxelProxy, FIntVector(100, 0, 0), 1}));
	const FVoxelFinePageKey Page{FIntVector::ZeroValue, 1};
	const FIntVector Sibling(1, 0, 0);
	auto FinePlan = MakeShared<FVoxelFinePagePlan, ESPMode::ThreadSafe>();
	FinePlan->SectionOwners.Add(FIntVector::ZeroValue, Page);
	FinePlan->SectionOwners.Add(Sibling, Page);
	Interest.FinePagePlan = FinePlan;
	Interest.CriticalFinePages.Add(Page);
	Interest.Exact.Add(Sibling, FVoxelExactDemand());
	Module->CurrentInterest = MakeShared<const FVoxelInterestSet, ESPMode::ThreadSafe>(Interest);
	FVoxelViewAdmission SiblingAdmission;
	SiblingAdmission.Kind = EVoxelViewAdmissionKind::Fine;
	SiblingAdmission.FineKey = Sibling;
	TestTrue(TEXT("Every Section needed by a critical Fine Page remains admissible"), Manager.IsCriticalDependency(SiblingAdmission));
	FVoxelSection* Section = Module->Runtime->FindOrAllocate(FIntVector::ZeroValue, 1);
	Section->Status = EVoxelSectionStatus::DataReady;
	Manager.FineReady.Add(FIntVector::ZeroValue);
	TestTrue(TEXT("A ready critical mesh keeps expansion gated until ownership is presented"), Manager.HasCriticalNearFieldPending());
	Manager.PresentedFineSectionOwners.Add(FIntVector::ZeroValue, Page);
	Manager.PresentedEmptyFinePages.Add(Page);
	TestFalse(TEXT("Presented empty ownership releases the critical near-field gate"), Manager.HasCriticalNearFieldPending());
	Interest.CriticalDependencies.Reset();
	Interest.CriticalFinePages.Reset();
	Interest.Exact.Reset();
	Module->CurrentInterest = MakeShared<const FVoxelInterestSet, ESPMode::ThreadSafe>(Interest);
	TestFalse(TEXT("Donation is withdrawn with its demand"), Manager.IsCriticalDependency(Dependency));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNegativeCoverageContinuationTest, "WHFramework.Voxel.Rendering.NegativeCoverageContinuation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelNegativeCoverageContinuationTest::RunTest(const FString& InParameters)
{
	FVoxelCoverageIndex Index;
	FString Error;
	for (int32 X : {-33, -2, -1, 0, 40})
	{
		auto Record = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
		Record->Geometry = {{EVoxelTerrainRepresentation::Fine, FIntVector(X, -1, 0), 0}, 1, -512, 512, 1, 1};
		Record->Geometry.Key.TryBounds(-512, 512, Record->Min, Record->Max);
		TestTrue(TEXT("Negative fixture is indexed"), Index.Upsert(Record, Error));
	}
	auto Upper = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
	Upper->Geometry = {{EVoxelTerrainRepresentation::Fine, FIntVector(-1, -1, 8), 0}, 1, -512, 512, 1, 1};
	Upper->Geometry.Key.TryBounds(-512, 512, Upper->Min, Upper->Max);
	TestTrue(TEXT("An independent Section in the same vertical column is indexed"), Index.Upsert(Upper, Error));
	FVoxelCoverageLocalSnapshot Sync;
	FVoxelCoverageLocalSnapshot Incremental;
	Index.Capture(FIntVector(-40, -20, -1), FIntVector(1, 1, 20), Sync);
	auto Steps = Index.CaptureSteps(FIntVector(-40, -20, -1), FIntVector(1, 1, 20), Incremental);
	int32 Operations = 0;
	while (!Steps.IsComplete() && Operations++ < 10000)
		Steps.Resume();
	TestTrue(TEXT("Inner traversal finishes under bounded resumptions"), Steps.IsComplete());
	TestTrue(TEXT("The query genuinely spans more than one microstep"), Operations > 1);
	TestEqual(TEXT("Negative query preserves coverage"), Incremental.Records.Num(), Sync.Records.Num());
	for (const auto& Record : Sync.Records)
		TestTrue(TEXT("Every synchronous record survives resumed capture"), Incremental.Records.Contains(Record));
	TestTrue(TEXT("Completed local read guards remain current"), Index.IsCurrent(Incremental));
	auto ChangedUpper = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>(*Upper);
	ChangedUpper->bReady = true;
	TestTrue(TEXT("The independent height changes readiness"), Index.Upsert(ChangedUpper, Error));
	TestTrue(TEXT("An unrelated height cannot invalidate a volume publication guard"), Index.IsCurrent(Incremental));
	if (!TestTrue(TEXT("Volume fixture contains a local input"), !Sync.Records.IsEmpty()))
	{
		return false;
	}
	auto Changed = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>(*Sync.Records[0]);
	Changed->bReady = true;
	Changed->Stamp.ContentRevision = 1;
	TestTrue(TEXT("A local readiness change is indexed"), Index.Upsert(Changed, Error));
	TestFalse(TEXT("A changed volume input still invalidates its publication guard"), Index.IsCurrent(Incremental));
	TestTrue(TEXT("The local input can return to its original values"), Index.Upsert(Sync.Records[0], Error));
	TestFalse(TEXT("An input change and restoration cannot revive a stale publication guard"), Index.IsCurrent(Incremental));
	FVoxelCoverageLocalSnapshot Full;
	auto FullSteps = Index.CaptureAllSteps(Full, false);
	while (!FullSteps.IsComplete())
	{
		FullSteps.Resume();
	}
	TestTrue(TEXT("The full index snapshot starts current"), Index.IsCurrent(Full));
	auto NewRoot = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
	NewRoot->Geometry = {{EVoxelTerrainRepresentation::Fine, FIntVector(1 << 20, 0, 0), 0}, 1, -512, 512, 1, 1};
	NewRoot->Geometry.Key.TryBounds(-512, 512, NewRoot->Min, NewRoot->Max);
	TestTrue(TEXT("A new root outside the captured root list is indexed"), Index.Upsert(NewRoot, Error));
	TestFalse(TEXT("Full index guards reject membership in a newly introduced root"), Index.IsCurrent(Full));
	const FVoxelCoverageRect Target{FIntPoint(-32, -16), FIntPoint(16, 16)};
	const TArray<FVoxelCoverageRect> Coverage{{FIntPoint(-32, -16), FIntPoint(0, 16)}, {FIntPoint(0, -16), FIntPoint(16, 16)}};
	bool bCovered = false;
	auto Query = VoxelCoverage::IsFullyCovered2DSteps(Target, Coverage, bCovered);
	while (!Query.IsComplete())
		Query.Resume();
	TestTrue(TEXT("Resumable subtraction covers negative and positive cells"), bCovered);
	const TArray<FVoxelCoverageRect> Gap{{FIntPoint(-32, -16), FIntPoint(-1, 16)}, {FIntPoint(0, -16), FIntPoint(16, 16)}};
	Query = VoxelCoverage::IsFullyCovered2DSteps(Target, Gap, bCovered);
	while (!Query.IsComplete())
		Query.Resume();
	TestFalse(TEXT("A one-cell hole is never accepted as covered"), bCovered);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainCostAndBudgetLivenessTest, "WHFramework.Voxel.Streaming.TerrainCostAndBudgetLiveness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelTerrainCostAndBudgetLivenessTest::RunTest(const FString& InParameters)
{
	FVoxelStreamingSource Source;
	Source.Id = FGuid::NewGuid();
	const FVoxelTerrainStageSource Stage{Source, 256, 64, 0};
	FVoxelTerrainStagePlan Plan;
	FString Error;
	for (const int32 Invalid : {16, 48, 256})
		TestFalse(TEXT("Proxy rejects invalid runtime sample caps"),
			FVoxelTerrainStagePlanner::Build(EVoxelTerrainRepresentation::VoxelProxy, MakeArrayView(&Stage, 1), 5, Invalid, -512, 512, 8.0, 1, 1, Plan, Error));
	FVoxelTerrainPageCostLimits Impossible;
	Impossible.Triangles = 1;
	TestFalse(TEXT("Unsplittable minimum legal page reports blocked cost"),
		FVoxelTerrainStagePlanner::Build(
			EVoxelTerrainRepresentation::VoxelProxy, MakeArrayView(&Stage, 1), 5, 32, -512, 512, 8.0, 1, 1, Plan, Error, nullptr, Impossible));
	FVoxelStreamingPerformanceBudget Settings;
	Settings.TerrainPublishMilliseconds = 0.1f;
	Settings.PublishOperationEstimateFloorMilliseconds = 0.01f;
	Settings.PublishOperationEstimateCeilingMilliseconds = 0.05f;
	TestTrue(TEXT("A small valid frame budget remains usable"), VoxelViewLod::ValidatePerformanceBudget(Settings, Error));
	FVoxelPublishBudget Budget;
	Budget.Begin(1.0, 0.1, 8);
	TestTrue(TEXT("A bounded microstep fits a small frame"), Budget.TryStart(EVoxelPublishPhase::Plan, 1.0, 0.01));
	Settings.PublishOperationEstimateFloorMilliseconds = 0.2f;
	TestFalse(TEXT("Floor larger than frame is rejected at runtime"), VoxelViewLod::ValidatePerformanceBudget(Settings, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCriticalReadyQueueLivenessTest, "WHFramework.Voxel.Streaming.CriticalReadyQueueLiveness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCriticalReadyQueueLivenessTest::RunTest(const FString& InParameters)
{
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	const auto Generator = MakeShared<FVoxelGenerationPipeline, ESPMode::ThreadSafe>(Config, Cache);
	const auto Registry = VoxelTest::MakeRegistry();
	FVoxelWorldRuntime Runtime(11, false, Registry, Generator);
	FVoxelTaskScheduler Scheduler;
	FVoxelRegionStore Store;
	FVoxelWorldManifest Manifest;
	FVoxelEmergeManager Manager(Runtime, Scheduler, Generator, Config, Cache, Store, Manifest, Registry);
	FVoxelInterestSet Interest;
	for (int32 Index = 0; Index < 256; ++Index)
	{
		const FIntVector Key(Index, 0, 0);
		FVoxelExactDemand Demand;
		Demand.bWarmupData = true;
		Demand.bSimulation = true;
		Interest.Exact.Add(Key, Demand);
		Interest.ExactOrder.Add(Key);
		Interest.ExactAdmissionLanes[0].Add(Index);
		Runtime.FindOrAllocate(Key, 0)->Status = EVoxelSectionStatus::BaseReady;
	}
	const FIntVector Tail(255, 0, 0);
	Runtime.FindSection(Tail)->Status = EVoxelSectionStatus::DataReady;
	for (int32 Frame = 0; Frame < 8 && !Runtime.FindSection(Tail)->bSimulationWanted; ++Frame)
	{
		++Interest.PriorityRevision;
		Manager.Tick(Interest, 1, Frame, {}, 1, 1000.0);
	}
	TestTrue(TEXT("Ready tail progresses behind blocked heads across continuous priority updates"), Runtime.FindSection(Tail)->bSimulationWanted);
	Manager.ReadyCriticalSections.Empty();
	Manager.QueuedCriticalSections.Reset();
	Manager.WakeCriticalSection(Tail);
	Manager.WakeCriticalSection(Tail);
	TestEqual(TEXT("Duplicate dependency wakeups share one ready entry"), Manager.QueuedCriticalSections.Num(), 1);
	Manager.Reset();
	Manager.WakeCriticalSection(Tail);
	TestTrue(TEXT("A removed consumer cannot be awakened"), Manager.QueuedCriticalSections.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelPlanConsumerWakeTest, "WHFramework.Voxel.Generation.PlanConsumerWake", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPlanConsumerWakeTest::RunTest(const FString& InParameters)
{
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	FVoxelTaskScheduler Scheduler;
	FVoxelGenerationPlanCoordinator Coordinator(Scheduler, Config, Cache, 11);
	FVoxelGenerationPlanDependency Dependency;
	Dependency.Kind = EVoxelGenerationPlanKind::Hydrology;
	Dependency.HydrologyKey.Coordinate = FIntPoint::ZeroValue;
	const FIntVector Critical(0, 0, 0);
	const FIntVector Visible(1, 0, 0);
	TSet<FIntVector> Woken;
	Coordinator.SetConsumerWakeCallback(
		[&Woken](const FIntVector& Consumer)
		{
			Woken.Add(Consumer);
		});
	Coordinator.Ensure(MakeArrayView(&Dependency, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Critical);
	Coordinator.Ensure(MakeArrayView(&Dependency, 1), EVoxelWorkClass::Visible, 3, 32.0, 0.0, Visible);
	TestEqual(TEXT("Consumers share one dependency attempt"), Scheduler.ActiveCount(), 1);
	Coordinator.ReleaseConsumer(Critical);
	TestEqual(TEXT("Critical reservation is returned after withdrawal"), Scheduler.GetDiagnostics().CriticalPending, 0);
	Scheduler.CancelMatching(
		[](EVoxelTaskKind Kind, const FVoxelTaskStamp&)
		{
			return Kind == EVoxelTaskKind::BuildGenerationPlan;
		});
	const double WakeDeadline = FPlatformTime::Seconds() + 2.0;
	while (!Woken.Contains(Visible) && FPlatformTime::Seconds() < WakeDeadline)
	{
		Scheduler.Tick(
			[](FVoxelTaskResult&&)
			{
			},
			8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	TestTrue(TEXT("Matching cancellation wakes the remaining consumer"), Woken.Contains(Visible));
	TestFalse(TEXT("Released consumer is not awakened by the old callback"), Woken.Contains(Critical));
	Coordinator.ReleaseConsumer(Visible);
	Scheduler.StopAndJoin();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelScopedCoverageContinuationTest, "WHFramework.Voxel.Rendering.ScopedCoverageContinuation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelScopedCoverageContinuationTest::RunTest(const FString& InParameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	UVoxelModule* Module = NewObject<UVoxelModule>();
	FVoxelTaskScheduler Scheduler;
	FVoxelViewPublisher Publisher(*Module, Scheduler, 11);
	AActor* Actor = World->SpawnActor<AActor>();
	const FVoxelPublishGroupKey Key{0, FIntVector::ZeroValue, 0};
	const FGuid Group = Publisher.BeginGroup(Key);
	auto& Entry = Publisher.Entries.Add(Actor);
	Entry.GroupKey = Key;
	Entry.Source = MakeShared<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
	Entry.Scale = Module->BlockSize();
	Entry.bHasDesiredExclusions = true;
	Entry.DesiredWorldExclusions.Add(FBox(FVector(-32, -32, -16), FVector(32, 32, 16)));
	const TArray<FBox> Scopes{FBox(FVector(-8, -8, -16), FVector(8, 8, 16))};
	bool bSuccess = false;
	auto Steps = Publisher.SetScopedCoverageSteps(Group, Actor, {}, nullptr, Scopes, bSuccess);
	int32 Operations = 0;
	while (!Steps.IsComplete() && Operations++ < 10000)
	{
		Steps.Resume();
	}
	TestTrue(TEXT("Scoped subtraction can pause inside an actor update"), Operations > 1 && Steps.IsComplete() && bSuccess);
	const auto& Boxes = Publisher.Entries.FindChecked(Actor).DesiredWorldExclusions;
	double Volume = 0.0;
	for (const FBox& Box : Boxes)
	{
		Volume += Box.GetVolume();
		TestFalse(TEXT("Removed local scope leaves no stale exclusion inside"), Box.IsInside(FVector::ZeroVector));
	}
	TestEqual(TEXT("Exclusions outside the local scope remain intact"), Volume, 64.0 * 64.0 * 32.0 - 16.0 * 16.0 * 32.0);
	Publisher.Reset();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPublishEstimateRecoveryTest, "WHFramework.Voxel.Rendering.PublishEstimateRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPublishEstimateRecoveryTest::RunTest(const FString& InParameters)
{
	UVoxelModule* Module = NewObject<UVoxelModule>();
	Module->ViewSettings.Budget.TerrainPublishMilliseconds = 0.1f;
	Module->ViewSettings.Budget.PublishOperationEstimateFloorMilliseconds = 0.01f;
	Module->ViewSettings.Budget.PublishOperationEstimateCeilingMilliseconds = 1.0;
	FVoxelTaskScheduler Scheduler;
	FVoxelViewPublisher Publisher(*Module, Scheduler, 11);
	const uint8 Phase = uint8(EVoxelPublishPhase::Plan);
	Publisher.PhaseOperationEstimateMilliseconds[Phase] = 8.0;
	FVoxelPublishBudget Budget;
	int32 Executed = 0;
	for (int32 Frame = 0; Frame < 64; ++Frame)
	{
		Budget.Begin(FPlatformTime::Seconds() - 1.0, 0.1, 8);
		Publisher.RunBudgetedPhase(Budget, EVoxelPublishPhase::Plan,
			[&Executed]
			{
				++Executed;
			});
	}
	TestEqual(TEXT("An expired hard deadline never executes work"), Executed, 0);
	TestEqual(TEXT("Consecutive deferrals remain observable"), Publisher.PhaseConsecutiveDeferrals[Phase], uint64(64));
	Budget.Begin(FPlatformTime::Seconds(), 0.1, 8);
	TestTrue(TEXT("A fresh small budget recovers without forcing an oversized estimate"), Publisher.RunBudgetedPhase(Budget, EVoxelPublishPhase::Plan,
																							  [&Executed]
																							  {
																								  ++Executed;
																							  }));
	TestEqual(TEXT("Successful progress clears consecutive deferrals"), Publisher.PhaseConsecutiveDeferrals[Phase], uint64(0));
	TestTrue(TEXT("The estimate is bounded by a valid frame fraction"), Publisher.PhaseOperationEstimateMilliseconds[Phase] <= 0.05);
	return true;
}

#endif
