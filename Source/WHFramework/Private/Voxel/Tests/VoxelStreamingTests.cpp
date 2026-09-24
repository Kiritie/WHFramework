#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Map/VoxelMapTileCache.h"
#include "HAL/PlatformProcess.h"
#include "Engine/Texture2D.h"
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

	Source.ExactRadius = 64;
	FVoxelViewSettings SmallWarmupSettings;
	SmallWarmupSettings.WarmupDataRadius = 12; // 600 cm at a 50 cm cell size.
	SmallWarmupSettings.WarmupCollisionRadius = 12;
	const FVoxelInterestSet SmallWarmup = Manager.Compute(
		MakeArrayView(&Source, 1), Manifest, SmallWarmupSettings);
	TestTrue(TEXT("Warmup includes the section containing a boundary-aligned source"),
		SmallWarmup.Warmup.Contains(FIntVector::ZeroValue));
	TestTrue(TEXT("Warmup collision includes the source section"),
		SmallWarmup.Exact.FindChecked(FIntVector::ZeroValue).bWarmupCollision);
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
	FVoxelStreamingLodSymmetryTest,
	"WHFramework.Voxel.Streaming.LodSymmetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingLodSymmetryTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -128;
	Manifest.Settings.MaxZ = 128;
	FVoxelViewSettings Settings;
	Settings.FineRadius = 64;
	Settings.FinePreload = 0;
	Settings.VoxelProxyRadius = 256;
	Settings.SurfaceRadius = 1024;
	Settings.MacroRadius = 16000;
	Settings.MaximumSurfaceTilesPerSource = 256;
	Settings.MaximumMacroTilesPerSource = 128;
	FVoxelStreamingSource Source;
	Source.ExactRadius = 64;
	Source.Center = FIntVector::ZeroValue;
	const FVoxelInterestManager Manager;
	const FVoxelInterestSet Interest = Manager.Compute(MakeArrayView(&Source, 1), Manifest, Settings);
	TestTrue(TEXT("Surface obeys tile budget"), Interest.Surface.Num() <= Settings.MaximumSurfaceTilesPerSource);
	TestTrue(TEXT("Macro obeys tile budget"), Interest.Macro.Num() <= Settings.MaximumMacroTilesPerSource);
	TestFalse(TEXT("Surface coverage remains available"), Interest.Surface.IsEmpty());
	TestFalse(TEXT("Macro coverage remains available"), Interest.Macro.IsEmpty());
	for (const FVoxelSurfaceTileKey& Key : Interest.Surface)
	{
		TestTrue(TEXT("Surface reflects across player X"), Interest.Surface.Contains({ FIntPoint(-Key.Coordinate.X - 1, Key.Coordinate.Y), Key.Level }));
		TestTrue(TEXT("Surface reflects across player Y"), Interest.Surface.Contains({ FIntPoint(Key.Coordinate.X, -Key.Coordinate.Y - 1), Key.Level }));
	}
	for (const FVoxelMacroTileKey& Key : Interest.Macro)
	{
		TestTrue(TEXT("Macro reflects across player X"), Interest.Macro.Contains({ FIntPoint(-Key.Coordinate.X - 1, Key.Coordinate.Y), Key.Level }));
		TestTrue(TEXT("Macro reflects across player Y"), Interest.Macro.Contains({ FIntPoint(Key.Coordinate.X, -Key.Coordinate.Y - 1), Key.Level }));
	}
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPersistedResidencyTest, "WHFramework.Voxel.Streaming.PersistedResidency", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPersistedResidencyTest::RunTest(const FString& InParameters)
{
	FVoxelWorldRuntime Runtime(4, true, VoxelTest::MakeRegistry(), VoxelTest::MakeGenerator());
	FVoxelTaskScheduler Scheduler;
	const FIntVector Key(-3, 2, 1);
	FVoxelSection* Section = Runtime.FindOrAllocate(Key, 0);
	if (!TestNotNull(TEXT("Edited section allocates"), Section))
	{
		return false;
	}
	Section->CommittedRevision = 7;
	Section->PersistedRevision = 6;
	Runtime.GetChangeIndex().SetModified(Key, true);
	FVoxelResidencyManager Manager(Runtime, Scheduler, {});
	Manager.SetEvictGraceFrames(0);
	const uint64 PreviousFrame = GFrameCounter;
	GFrameCounter += 15;
	Manager.Tick({}, 1, 0.0);
	TestNotNull(TEXT("Unpersisted edits remain resident"), Runtime.FindSection(Key));
	Runtime.MarkCommitted(Key, 6);
	GFrameCounter += 15;
	Manager.Tick({}, 1, 0.0);
	TestNotNull(TEXT("Stale save completion cannot permit eviction"), Runtime.FindSection(Key));
	Runtime.MarkCommitted(Key, 7);
	GFrameCounter += 15;
	Manager.Tick({}, 1, 0.0);
	GFrameCounter = PreviousFrame;
	TestNull(TEXT("Persisted modified section may unload"), Runtime.FindSection(Key));
	TestTrue(TEXT("Eviction retains the persistent change index"), Runtime.GetChangeIndex().IsModified(Key));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelFineLodHysteresisTest,
	"WHFramework.Voxel.Streaming.FineLodHysteresis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFineLodHysteresisTest::RunTest(const FString& InParameters)
{
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -128;
	Manifest.Settings.MaxZ = 128;
	FVoxelViewSettings Settings;
	Settings.FineRadius = 64;
	Settings.FinePreload = 0;
	FVoxelStreamingSource Source;
	Source.ExactRadius = 64;
	Source.VerticalExactRadius = 16;
	Source.RenderMode = EVoxelStreamingRenderMode::FineOnly;
	const FVoxelInterestManager Manager;
	const FIntVector Key(3, 0, 0);
	const FVoxelInterestSet Initial = Manager.Compute(MakeArrayView(&Source, 1), Manifest, Settings);
	TestTrue(TEXT("Approaching enters fine LOD"), Initial.Exact.FindChecked(Key).bFineRender);
	Source.Center.X = -16;
	const FVoxelInterestSet Retained = Manager.Compute(MakeArrayView(&Source, 1), Manifest, Settings, &Initial);
	TestTrue(TEXT("Small retreat retains fine LOD"), Retained.Exact.FindChecked(Key).bFineRender);
	Source.Center.X = -32;
	const FVoxelInterestSet Retired = Manager.Compute(MakeArrayView(&Source, 1), Manifest, Settings, &Retained);
	const FVoxelExactDemand* Demand = Retired.Exact.Find(Key);
	TestFalse(TEXT("Retreat beyond hysteresis releases fine LOD"), Demand && Demand->bFineRender);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStreamingSchedulerCriticalFirstTest,
	"WHFramework.Voxel.Streaming.Scheduler.CriticalFirst",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingSchedulerCriticalFirstTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelTaskRequest Near;
	Near.Kind = EVoxelTaskKind::GenerateExactBase;
	Near.WorkClass = EVoxelWorkClass::ExactData;
	Near.DistanceScore = 32.0;
	FVoxelTaskRequest Far;
	Far.Kind = EVoxelTaskKind::BuildCollision;
	Far.WorkClass = EVoxelWorkClass::Critical;
	Far.DistanceScore = 512.0;
	TestTrue(TEXT("Critical support outranks ordinary nearer generation"), FVoxelTaskScheduler::IsHigherPriority(Far, Near));
	TestFalse(TEXT("Ordinary generation cannot starve critical support"), FVoxelTaskScheduler::IsHigherPriority(Near, Far));
	FVoxelTaskRequest Visual;
	Visual.Kind = EVoxelTaskKind::BuildVoxelProxy;
	Visual.WorkClass = EVoxelWorkClass::Visible;
	Visual.DistanceScore = 0.0;
	TestTrue(TEXT("Critical work outranks zero-distance visual work"), FVoxelTaskScheduler::IsHigherPriority(Far, Visual));
	TestTrue(TEXT("Required data outranks visual work"), FVoxelTaskScheduler::IsHigherPriority(Near, Visual));
	const FVoxelTaskRequest* Requests[] = { &Near, &Far, &Visual };
	for (const FVoxelTaskRequest* First : Requests)
	{
		for (const FVoxelTaskRequest* Second : Requests)
		{
			for (const FVoxelTaskRequest* Third : Requests)
			{
				if (FVoxelTaskScheduler::IsHigherPriority(*First, *Second) && FVoxelTaskScheduler::IsHigherPriority(*Second, *Third))
				{
					TestTrue(TEXT("Mixed data collision and view priorities are transitive"), FVoxelTaskScheduler::IsHigherPriority(*First, *Third));
				}
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFineIndependentRadiusTest, "WHFramework.Voxel.Streaming.FineRadiusIndependentOfInteraction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFineIndependentRadiusTest::RunTest(const FString& InParameters)
{
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -256;
	Manifest.Settings.MaxZ = 256;
	FVoxelViewSettings Settings;
	Settings.FineRadius = 96;
	Settings.FinePreload = 16;
	Settings.FineVerticalRadius = 32;
	FVoxelStreamingSource Source;
	Source.ExactRadius = 16;
	Source.CollisionRadius = 16;
	Source.VerticalExactRadius = 16;
	Source.RenderMode = EVoxelStreamingRenderMode::FineOnly;
	const FVoxelInterestSet Interest = FVoxelInterestManager().Compute(MakeArrayView(&Source, 1), Manifest, Settings);
	for (const FIntVector Key : { FIntVector(5, 0, 0), FIntVector(-6, 0, 0), FIntVector(0, 0, 1) })
	{
		const FVoxelExactDemand* Demand = Interest.Exact.Find(Key);
		TestTrue(TEXT("Configured fine volume includes nearby horizontal and vertical sections"), Demand && Demand->bFineRender);
		TestFalse(TEXT("Fine range does not enlarge collision or simulation"), Demand && (Demand->bCollision || Demand->bSimulation));
	}
	const FVoxelExactDemand* DeepDemand = Interest.Exact.Find(FIntVector(0, 0, 5));
	TestFalse(TEXT("Fine vertical radius excludes deep invisible sections"), DeepDemand && DeepDemand->bFineRender);
	TestFalse(TEXT("Deep invisible sections never enter fine admissions"),
		Interest.FineSections && Interest.FineSections->Contains(FIntVector(0, 0, 5)));
	for (const auto& Pair : Interest.Exact)
	{
		if (!Pair.Value.bFineRender) continue;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			for (const int32 Sign : { -1, 1 })
			{
				FIntVector Neighbor = Pair.Key;
				Neighbor[Axis] += Sign;
				TestTrue(TEXT("Every fine boundary has a requested real data halo"), Interest.Exact.Contains(Neighbor));
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelSchedulerAdmissionTest, "WHFramework.Voxel.Streaming.Scheduler.PriorityAdmission", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSchedulerAdmissionTest::RunTest(const FString& InParameters)
{
	FVoxelTaskScheduler Scheduler;
	FVoxelTaskBudget Budget;
	Budget.MaxConcurrentTasks = 1;
	Budget.MaxPendingTasks = 1;
	Scheduler.SetBudget(Budget);
	auto MakeRequest = [](const uint64 Token, const EVoxelWorkClass WorkClass, const double Distance)
	{
		FVoxelTaskRequest Request;
		Request.Kind = EVoxelTaskKind::GenerateExactBase;
		Request.WorkClass = WorkClass;
		Request.Stamp.WorldEpoch = 1;
		Request.Stamp.Token = Token;
		Request.DistanceScore = Distance;
		Request.ReservedBytes = 1024;
		Request.Execute = [](const TAtomic<bool>& Cancel)
		{
			while (!Cancel.Load())
			{
				FPlatformProcess::Sleep(0.001f);
			}
			return FVoxelTaskResult();
		};
		return Request;
	};
	FVoxelTaskRequest Running = MakeRequest(1, EVoxelWorkClass::Visible, 1.0);
	TestTrue(TEXT("Worker is occupied"), Scheduler.Enqueue(MoveTemp(Running)));
	FVoxelTaskRequest Far = MakeRequest(2, EVoxelWorkClass::Visible, 1000.0);
	const FVoxelTaskStamp FarStamp = Far.Stamp;
	int32 CanceledCallbacks = 0;
	Far.Apply = [&CanceledCallbacks](FVoxelTaskResult&& Result)
	{
		CanceledCallbacks += Result.bCanceled ? 1 : 0;
	};
	TestTrue(TEXT("Far work fills the pending queue"), Scheduler.Enqueue(MoveTemp(Far)));
	FVoxelTaskRequest Critical = MakeRequest(3, EVoxelWorkClass::Critical, 10.0);
	const FVoxelTaskStamp CriticalStamp = Critical.Stamp;
	TestTrue(TEXT("Critical work can enter a full queue"), Scheduler.Enqueue(MoveTemp(Critical)));
	TestFalse(TEXT("Displaced work is removed from active bookkeeping"), Scheduler.Has(FarStamp, EVoxelTaskKind::GenerateExactBase));
	TestTrue(TEXT("Critical work remains queued"), Scheduler.Has(CriticalStamp, EVoxelTaskKind::GenerateExactBase));
	Scheduler.Tick([](FVoxelTaskResult&&) {});
	TestEqual(TEXT("Displaced owner receives cancellation exactly once"), CanceledCallbacks, 1);
	TestEqual(TEXT("Pending queue stays bounded"), Scheduler.GetDiagnostics().Pending, 1);
	Scheduler.StopAndJoin();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelSchedulerTerrainReserveTest,
	"WHFramework.Voxel.Streaming.Scheduler.TerrainLaneAdmission", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSchedulerTerrainReserveTest::RunTest(const FString& Parameters)
{
	FVoxelTaskScheduler Scheduler;
	FVoxelTaskBudget Budget;
	Budget.MaxConcurrentTasks = 1;
	Budget.MaxPendingTasks = 16;
	Scheduler.SetBudget(Budget);
	auto MakeRequest = [](const uint64 Token, const EVoxelTaskKind Kind, const double Distance)
	{
		FVoxelTaskRequest Request;
		Request.Kind = Kind;
		Request.WorkClass = EVoxelWorkClass::Visible;
		Request.Stamp.Token = Token;
		Request.DistanceScore = Distance;
		Request.ReservedBytes = 1024;
		Request.Execute = [](const TAtomic<bool>& Cancel)
		{
			while (!Cancel.Load()) FPlatformProcess::Sleep(0.001f);
			return FVoxelTaskResult();
		};
		return Request;
	};
	Scheduler.Enqueue(MakeRequest(1, EVoxelTaskKind::GenerateExactBase, 0.0));
	for (int32 Index = 0; Index < 8; ++Index)
	{
		TestTrue(TEXT("Ordinary generation uses its available slots"), Scheduler.Enqueue(MakeRequest(2 + Index, EVoxelTaskKind::GenerateExactBase, Index)));
	}
	TestFalse(TEXT("Generation cannot consume every representation slot"), Scheduler.Enqueue(MakeRequest(20, EVoxelTaskKind::GenerateExactBase, 100.0)));
	const EVoxelTaskKind Kinds[] = { EVoxelTaskKind::BuildFineMesh, EVoxelTaskKind::BuildVoxelProxy, EVoxelTaskKind::BuildSurface, EVoxelTaskKind::BuildMacro };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		TestTrue(TEXT("Each representation can enter under generation pressure"), Scheduler.Enqueue(MakeRequest(30 + Index, Kinds[Index], 10000.0)));
	}
	TestTrue(TEXT("Shared pending queue remains bounded"), Scheduler.GetDiagnostics().Pending <= Budget.MaxPendingTasks);
	Scheduler.StopAndJoin();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelEmptyMeshApplyTest,
	"WHFramework.Voxel.Streaming.Scheduler.EmptyMeshApply", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelEmptyMeshApplyTest::RunTest(const FString& Parameters)
{
	FVoxelTaskResult Result;
	Result.Kind = EVoxelTaskKind::BuildFineMesh;
	Result.bSuccess = true;
	Result.FineMesh = MakeShared<FVoxelSectionMeshResult>();
	TestFalse(TEXT("Known empty sections do not block heavy mesh publication"), Result.HasHeavyApply());
	Result.FineMesh->Batches.AddDefaulted_GetRef().Mesh.Triangles = { 0, 1, 2 };
	TestTrue(TEXT("Renderable mesh still obeys the heavy result budget"), Result.HasHeavyApply());
	Result.bCanceled = true;
	TestFalse(TEXT("Canceled geometry does not consume publication quota"), Result.HasHeavyApply());
	Result.bCanceled = false;
	Result.Kind = EVoxelTaskKind::BuildCollision;
	TestTrue(TEXT("Collision keeps its heavy apply protection"), Result.HasHeavyApply());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelMapTileCoverageTest,
	"WHFramework.Voxel.Map.TileCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelMapTileCoverageTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	TestEqual(TEXT("Near map step"), FVoxelMapTileCache::SelectStep(0.3f), 16);
	TestEqual(TEXT("Regional map step"), FVoxelMapTileCache::SelectStep(0.02f), 512);
	FSceneMapView View;
	View.Center = FVector2D(-25.0, -25.0);
	View.Range = 500000.f;
	View.Yaw = 45.f;
	TArray<FVoxelMapTileKey> Keys;
	FVoxelMapTileCache::VisibleTiles(View, FVector2D(1000.0, 700.0), 25.0, Keys);
	TestTrue(TEXT("Rotated view includes negative tile coordinates"),
		Keys.ContainsByPredicate([](const FVoxelMapTileKey& Key)
		{
			return Key.Coordinate.X < 0 && Key.Coordinate.Y < 0;
		}));
	TestTrue(TEXT("Map tile count stays bounded"), Keys.Num() > 0 && Keys.Num() <= 1089);
	for (const FVoxelMapTileKey& Key : Keys)
	{
		TestEqual(TEXT("All visible tiles share one LOD"), Key.Step, 128);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelMapTileBuildTest,
	"WHFramework.Voxel.Map.BackgroundTileBuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelMapTileBuildTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Generator = MakeShared<const FVoxelGenerationPipeline, ESPMode::ThreadSafe>(
		Config, MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>());
	FVoxelTaskScheduler Scheduler;
	FVoxelMapTileCache Cache(Scheduler, Generator, Config, 1, 25.0);
	const FVoxelMapTileKey Key { FIntPoint(-1, 0), 16 };
	const FVoxelMapTileKey WorldKey { FIntPoint(0, 0), 512 };
	TArray<FVoxelMapTileKey> Requested;
	Requested.Add(Key);
	Requested.Add(WorldKey);
	FSceneMapView View;
	Cache.Request(Requested, View);
	const double Deadline = FPlatformTime::Seconds() + 90.0;
	while ((!Cache.FindBrush(Key) || !Cache.FindBrush(WorldKey)) &&
		FPlatformTime::Seconds() < Deadline)
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.01f);
	}
	const FSlateBrush* Brush = Cache.FindBrush(Key);
	TestNotNull(TEXT("Natural tile reaches the texture cache"), Brush);
	TestNotNull(TEXT("20 km world map tile reaches the texture cache"),
		Cache.FindBrush(WorldKey));
	if (Brush)
	{
		const UTexture2D* Texture = Cast<UTexture2D>(Brush->GetResourceObject());
		TestNotNull(TEXT("Tile has an actual texture"), Texture);
		if (Texture)
		{
			TestEqual(TEXT("Tile texture width"), Texture->GetSizeX(), FVoxelMapTileCache::TileSide);
			TestEqual(TEXT("Tile texture height"), Texture->GetSizeY(), FVoxelMapTileCache::TileSide);
		}
	}
	Scheduler.StopAndJoin();
	return true;
}

#endif
