#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Voxel/Generation/VoxelNaturalGenerationCache.h"
#include "Voxel/Streaming/VoxelInterest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelNaturalTileKeyTest,
	"WHFramework.Voxel.Performance.NaturalTileKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNaturalTileKeyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FVoxelNaturalTileKey A = VoxelNaturalTileKeyFromCell(0, 0);
	const FVoxelNaturalTileKey B = VoxelNaturalTileKeyFromCell(31, 31);
	const FVoxelNaturalTileKey C = VoxelNaturalTileKeyFromCell(32, 0);
	const FVoxelNaturalTileKey D = VoxelNaturalTileKeyFromCell(-1, -1);
	TestTrue(TEXT("0 and 31 share one river field tile"), A == B);
	TestTrue(TEXT("32 enters next river field tile"), A != C);
	TestEqual(TEXT("Negative X floor divides correctly"), D.Coordinate.X, -1);
	TestEqual(TEXT("Negative Y floor divides correctly"), D.Coordinate.Y, -1);
	return true;
}

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
