#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "EngineGlobals.h"
#include "Voxel/Streaming/VoxelInterestManager.h"
#include "Voxel/Streaming/VoxelResidencyManager.h"
#include "Voxel/Rendering/VoxelViewManager.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStreamingDemandTest,
	"WHFramework.Voxel.Streaming.DemandSeparation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingDemandTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -128;
	Manifest.Settings.MaxZ = 128;
	FVoxelStreamingSource Source;
	Source.Center = FIntVector::ZeroValue;
	Source.ExactRadius = 0;
	Source.CollisionRadius = 24;
	Source.bCollision = true;
	Source.RenderMode = EVoxelStreamingRenderMode::None;

	const FVoxelInterestManager Manager;
	const FVoxelInterestSet Interest = Manager.Compute(
		MakeArrayView(&Source, 1),
		Manifest,
		FVoxelViewSettings());
	TestTrue(TEXT("Collision demand creates exact data"), Interest.Exact.Contains(FIntVector::ZeroValue));
	const FVoxelExactDemand& Demand = Interest.Exact.FindChecked(FIntVector::ZeroValue);
	TestTrue(TEXT("Collision demand is marked collision"), Demand.bCollision);
	TestFalse(TEXT("Collision-only demand is not in data warmup"), Demand.bWarmupData);
	TestTrue(TEXT("Collision demand is in warmup collision"), Demand.bWarmupCollision);
	TestFalse(TEXT("Collision-only demand is not marked exact"), Demand.bExact);
	TestFalse(TEXT("Collision demand does not imply fine render"), Demand.bFineRender);
	TestTrue(TEXT("Non-render source creates no voxel proxies"), Interest.VoxelProxy.IsEmpty());
	TestTrue(TEXT("Non-render source creates no surface tiles"), Interest.Surface.IsEmpty());
	TestTrue(TEXT("Non-render source creates no macro tiles"), Interest.Macro.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStreamingRenderScopeTest,
	"WHFramework.Voxel.Streaming.RenderScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingRenderScopeTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -128;
	Manifest.Settings.MaxZ = 128;
	FVoxelViewSettings Settings;
	FVoxelStreamingSource Source;
	Source.Center = FIntVector::ZeroValue;
	Source.ExactRadius = 32;
	Source.CollisionRadius = 32;
	Source.VerticalExactRadius = 16;
	Source.RenderMode = EVoxelStreamingRenderMode::FineOnly;

	const FVoxelInterestManager Manager;
	const FVoxelInterestSet FineOnly = Manager.Compute(
		MakeArrayView(&Source, 1),
		Manifest,
		Settings);
	bool bHasFine = false;
	for (const TPair<FIntVector, FVoxelExactDemand>& Pair : FineOnly.Exact)
	{
		bHasFine |= Pair.Value.bFineRender;
	}
	TestTrue(TEXT("FineOnly creates fine demand"), bHasFine);
	TestTrue(TEXT("FineOnly creates no voxel proxies"), FineOnly.VoxelProxy.IsEmpty());
	TestTrue(TEXT("FineOnly creates no surface tiles"), FineOnly.Surface.IsEmpty());
	TestTrue(TEXT("FineOnly creates no macro tiles"), FineOnly.Macro.IsEmpty());

	Source.RenderMode = EVoxelStreamingRenderMode::Full;
	const FVoxelInterestSet Full = Manager.Compute(
		MakeArrayView(&Source, 1),
		Manifest,
		Settings);
	TestFalse(TEXT("Full creates voxel proxies"), Full.VoxelProxy.IsEmpty());
	TestFalse(TEXT("Full creates surface tiles"), Full.Surface.IsEmpty());
	TestFalse(TEXT("Full creates macro tiles"), Full.Macro.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelResidencyPinTest,
	"WHFramework.Voxel.Streaming.ResidencyPin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelResidencyPinTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldRuntime Runtime(4, false, VoxelTest::MakeRegistry(), VoxelTest::MakeGenerator());
	FVoxelTaskScheduler Scheduler;
	const FIntVector SectionKey(3, -2, 1);
	FVoxelSection* Section = Runtime.FindOrAllocate(SectionKey, 0);
	TestNotNull(TEXT("Residency test section allocates"), Section);
	if (!Section)
	{
		return false;
	}
	Section->PinCount.Store(1);
	int32 Evictions = 0;
	FVoxelResidencyManager Manager(Runtime, Scheduler, [&Evictions](const FIntVector& InSection)
	{
		(void)InSection;
		++Evictions;
	});
	Manager.SetEvictGraceFrames(0);
	Manager.Tick({}, 1, 0.0);
	TestNotNull(TEXT("Pinned section is retained"), Runtime.FindSection(SectionKey));
	TestEqual(TEXT("Pinned section does not run eviction callback"), Evictions, 0);

	Section->PinCount.Store(0);
	const uint64 PreviousFrameCounter = GFrameCounter;
	GFrameCounter += 15;
	Manager.Tick({}, 1, 0.0);
	GFrameCounter = PreviousFrameCounter;
	TestNull(TEXT("Unpinned undemanded section evicts"), Runtime.FindSection(SectionKey));
	TestEqual(TEXT("Eviction callback runs once"), Evictions, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStreamingViewNearestFirstTest,
	"WHFramework.Voxel.Streaming.View.NearestFirst",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingViewNearestFirstTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	TArray<FVoxelViewAdmission> Admissions;
	Admissions.Add({ EVoxelViewAdmissionKind::Macro, 512.0 });
	Admissions.Add({ EVoxelViewAdmissionKind::Surface, 32.0 });
	Admissions.Add({ EVoxelViewAdmissionKind::VoxelProxy, 128.0 });
	FVoxelViewManager::SortAdmissionsByPriority(Admissions);
	TestEqual(TEXT("Nearest admission is first"), Admissions[0].DistanceCells, 32.0);
	TestEqual(TEXT("Middle admission is second"), Admissions[1].DistanceCells, 128.0);
	TestEqual(TEXT("Farthest admission is last"), Admissions[2].DistanceCells, 512.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStreamingViewFrontierBlocksFarTileTest,
	"WHFramework.Voxel.Streaming.View.FrontierBlocksFarTile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingViewFrontierBlocksFarTileTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const TArray<FVoxelViewAdmission> Admissions = {
		{ EVoxelViewAdmissionKind::Fine, 64.0 },
		{ EVoxelViewAdmissionKind::Surface, 256.0 }
	};
	const double Frontier = FVoxelViewManager::ResolveAdmissionFrontier(
		Admissions,
		[](const FVoxelViewAdmission&) { return false; },
		64.0);
	TestEqual(TEXT("Nearest unfinished ring defines the frontier"), Frontier, 128.0);
	TestTrue(TEXT("Far tile is outside the frontier"), Admissions[1].DistanceCells > Frontier);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStreamingViewFrontierAdvancesTest,
	"WHFramework.Voxel.Streaming.View.FrontierAdvances",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingViewFrontierAdvancesTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const TArray<FVoxelViewAdmission> Admissions = {
		{ EVoxelViewAdmissionKind::Fine, 32.0 },
		{ EVoxelViewAdmissionKind::VoxelProxy, 96.0 },
		{ EVoxelViewAdmissionKind::Surface, 192.0 }
	};
	const double Initial = FVoxelViewManager::ResolveAdmissionFrontier(
		Admissions,
		[](const FVoxelViewAdmission&) { return false; },
		64.0);
	const double Advanced = FVoxelViewManager::ResolveAdmissionFrontier(
		Admissions,
		[](const FVoxelViewAdmission& Admission) { return Admission.DistanceCells < 192.0; },
		64.0);
	TestEqual(TEXT("Initial frontier covers only the nearest band"), Initial, 96.0);
	TestEqual(TEXT("Ready near rings advance the frontier"), Advanced, 256.0);
	TestTrue(TEXT("Far ring becomes admissible"), Admissions[2].DistanceCells <= Advanced);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStreamingViewDistanceScoreTest,
	"WHFramework.Voxel.Streaming.View.DistanceScore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingViewDistanceScoreTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -128;
	Manifest.Settings.MaxZ = 128;
	FVoxelStreamingSource Source;
	Source.Center = FIntVector::ZeroValue;
	Source.ExactRadius = 64;
	Source.CollisionRadius = 64;
	Source.RenderMode = EVoxelStreamingRenderMode::FineOnly;
	const FVoxelInterestSet Interest = FVoxelInterestManager().Compute(
		MakeArrayView(&Source, 1), Manifest, FVoxelViewSettings());
	const FVoxelExactDemand* Center = Interest.Exact.Find(FIntVector::ZeroValue);
	TestNotNull(TEXT("Center demand exists"), Center);
	double MinimumDistance = TNumericLimits<double>::Max();
	double MaximumDistance = 0.0;
	for (const TPair<FIntVector, FVoxelExactDemand>& Pair : Interest.Exact)
	{
		TestTrue(TEXT("Demand distance is finite"), FMath::IsFinite(Pair.Value.DistanceCells));
		MinimumDistance = FMath::Min(MinimumDistance, Pair.Value.DistanceCells);
		MaximumDistance = FMath::Max(MaximumDistance, Pair.Value.DistanceCells);
	}
	if (Center)
	{
		TestEqual(TEXT("Observer section has the minimum center distance"), Center->DistanceCells, MinimumDistance);
	}
	TestTrue(TEXT("Off-center demand carries a larger real distance score"), MaximumDistance > MinimumDistance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStreamingSchedulerVisualDistanceFirstTest,
	"WHFramework.Voxel.Streaming.Scheduler.VisualDistanceFirst",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingSchedulerVisualDistanceFirstTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelTaskRequest Near;
	Near.WorkClass = EVoxelWorkClass::Background;
	Near.DistanceScore = 32.0;
	FVoxelTaskRequest Far;
	Far.WorkClass = EVoxelWorkClass::Visible;
	Far.DistanceScore = 512.0;
	TestTrue(TEXT("Near visual work outranks far visual work"), FVoxelTaskScheduler::IsHigherPriority(Near, Far));
	TestFalse(TEXT("Far visual work does not outrank near visual work"), FVoxelTaskScheduler::IsHigherPriority(Far, Near));
	return true;
}

#endif
