#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Streaming/VoxelInterestManager.h"
#include "Voxel/Streaming/VoxelResidencyManager.h"
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
	Source.bRender = false;

	const FVoxelInterestManager Manager;
	const FVoxelInterestSet Interest = Manager.Compute(
		MakeArrayView(&Source, 1),
		Manifest,
		FVoxelViewSettings());
	TestTrue(TEXT("Collision demand creates exact data"), Interest.Exact.Contains(FIntVector::ZeroValue));
	const FVoxelExactDemand& Demand = Interest.Exact.FindChecked(FIntVector::ZeroValue);
	TestTrue(TEXT("Collision demand is marked collision"), Demand.bCollision);
	TestFalse(TEXT("Collision-only demand is not marked exact"), Demand.bExact);
	TestFalse(TEXT("Collision demand does not imply fine render"), Demand.bFineRender);
	TestTrue(TEXT("Non-render source creates no voxel proxies"), Interest.VoxelProxy.IsEmpty());
	TestTrue(TEXT("Non-render source creates no surface tiles"), Interest.Surface.IsEmpty());
	TestTrue(TEXT("Non-render source creates no macro tiles"), Interest.Macro.IsEmpty());
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
	Manager.Tick({}, 0.0);
	TestNotNull(TEXT("Pinned section is retained"), Runtime.FindSection(SectionKey));
	TestEqual(TEXT("Pinned section does not run eviction callback"), Evictions, 0);

	Section->PinCount.Store(0);
	Manager.Tick({}, 0.0);
	TestNull(TEXT("Unpinned undemanded section evicts"), Runtime.FindSection(SectionKey));
	TestEqual(TEXT("Eviction callback runs once"), Evictions, 1);
	return true;
}

#endif
