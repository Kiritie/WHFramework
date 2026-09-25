#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Map/VoxelMapSurfaceResolver.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelMapSurfaceResolverTest,
	"WHFramework.Voxel.Map.UnloadedSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelMapSurfaceResolverTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const TSharedRef<FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator =
		VoxelTest::MakeGenerator(173);
	const FVector2D MapPosition(-19537.25, 31208.5);
	FVector First;
	FVector Second;
	FString Error;
	if (!TestTrue(TEXT("Unloaded map coordinate resolves"),
		FVoxelMapSurfaceResolver::Resolve(*Generator, 25.0, MapPosition, First, Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Clicked X is preserved"), First.X, MapPosition.X);
	TestEqual(TEXT("Clicked Y is preserved"), First.Y, MapPosition.Y);
	TestTrue(TEXT("Repeat resolution succeeds"),
		FVoxelMapSurfaceResolver::Resolve(*Generator, 25.0, MapPosition, Second, Error));
	TestEqual(TEXT("Resolution is deterministic"), First, Second);

	FVoxelEnvironmentSample Environment;
	const int32 CellX = FMath::FloorToInt(MapPosition.X / 25.0);
	const int32 CellY = FMath::FloorToInt(MapPosition.Y / 25.0);
	if (!TestTrue(TEXT("Natural environment is queryable without a resident section"),
		Generator->SampleEnvironment(CellX, CellY, Environment, Error)))
	{
		AddError(Error);
		return false;
	}
	int32 ExpectedTop = Environment.Column.SurfaceZ;
	if (Environment.Column.bRiver || Environment.Column.bLake || Environment.Column.bOcean)
	{
		ExpectedTop = FMath::Max(ExpectedTop, Environment.Column.SurfaceWaterZ);
	}
	TestEqual(TEXT("Marker clears natural land or water surface"),
		First.Z, (ExpectedTop + 2.0) * 25.0);
	TestFalse(TEXT("Zero block scale is rejected"),
		FVoxelMapSurfaceResolver::Resolve(*Generator, 0.0, MapPosition, Second, Error));
	TestFalse(TEXT("Out-of-range world coordinate is rejected"),
		FVoxelMapSurfaceResolver::Resolve(*Generator, 25.0, FVector2D(1.0e20, 0.0), Second, Error));
	return true;
}

#endif
