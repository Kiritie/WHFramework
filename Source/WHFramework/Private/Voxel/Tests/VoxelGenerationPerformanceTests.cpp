#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Voxel/Generation/VoxelNaturalGenerationCache.h"
#include "Voxel/Streaming/VoxelInterest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelWarmupDemandDefaultsTest,
	"WHFramework.Voxel.Performance.WarmupDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelWarmupDemandDefaultsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FVoxelViewSettings Settings;
	TestTrue(TEXT("Warmup data is smaller than default fine radius"), Settings.WarmupDataRadius < Settings.FineRadius);
	TestTrue(TEXT("Warmup collision is smaller than default fine radius"), Settings.WarmupCollisionRadius < Settings.FineRadius);
	return true;
}

#endif
