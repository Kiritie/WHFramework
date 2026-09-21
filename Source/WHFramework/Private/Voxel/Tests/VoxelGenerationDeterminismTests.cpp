#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelGenerationMathDeterminismTest,
	"WHFramework.Voxel.Generation.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelGenerationMathDeterminismTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	static const FIntVector Coordinates[] =
	{
		FIntVector::ZeroValue,
		FIntVector(1, -1, 0),
		FIntVector(-16, 32, 4),
		FIntVector(128, -64, -3)
	};
	for (const FIntVector& Coordinate : Coordinates)
	{
		const uint64 SeedA = VoxelGeneration::MakeSeed(173, Coordinate, 0x9e3779b97f4a7c15ull);
		const uint64 SeedB = VoxelGeneration::MakeSeed(173, Coordinate, 0x9e3779b97f4a7c15ull);
		TestEqual(TEXT("Fixed seed and coordinate are stable"), SeedA, SeedB);
		TestEqual(
			TEXT("3D noise is stable"),
			VoxelGeneration::Noise3D(173, Coordinate, 64, 31),
			VoxelGeneration::Noise3D(173, Coordinate, 64, 31));
	}
	TestEqual(TEXT("Negative floor division"), VoxelGeneration::FloorDivide(-1, 16), -1);
	TestEqual(TEXT("Negative exact floor division"), VoxelGeneration::FloorDivide(-16, 16), -1);
	TestTrue(
		TEXT("Different seed changes stable identity"),
		VoxelGeneration::MakeStableId(173, Coordinates[2], 7) !=
		VoxelGeneration::MakeStableId(174, Coordinates[2], 7));

	const TSharedRef<FVoxelGenerationPipeline, ESPMode::ThreadSafe> GeneratorA = VoxelTest::MakeGenerator();
	const TSharedRef<FVoxelGenerationPipeline, ESPMode::ThreadSafe> GeneratorB = VoxelTest::MakeGenerator();
	for (const FIntVector& Coordinate : Coordinates)
	{
		TArray<FVoxelBlockState> BaseA;
		TArray<FVoxelBlockState> BaseB;
		FString ErrorA;
		FString ErrorB;
		TestTrue(TEXT("First exact base generates"), GeneratorA->GenerateSection(Coordinate, BaseA, ErrorA));
		TestTrue(TEXT("Second exact base generates"), GeneratorB->GenerateSection(Coordinate, BaseB, ErrorB));
		TestEqual(TEXT("Exact base cell count is stable"), BaseA.Num(), BaseB.Num());
		TestEqual(TEXT("Exact base fingerprint is stable"), VoxelTest::HashBlocks(BaseA), VoxelTest::HashBlocks(BaseB));
	}
	FVoxelBlockState SampleA;
	FVoxelBlockState SampleB;
	FString SampleError;
	TestTrue(TEXT("Negative coordinate first query"), GeneratorA->SampleBlock(FIntVector(-17, 31, -9), SampleA, SampleError));
	TestTrue(TEXT("Negative coordinate second query"), GeneratorB->SampleBlock(FIntVector(-17, 31, -9), SampleB, SampleError));
	TestEqual(TEXT("Query bounds do not change natural cell"), SampleA.Pack(), SampleB.Pack());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelSurfaceCandidateSupportTest,
	"WHFramework.Voxel.Generation.SurfaceCandidateSupport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSurfaceCandidateSupportTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationRuntimeConfig RuntimeConfig = *VoxelTest::MakeGenerationConfig();
	FVoxelGenerationRecipe Recipe = *RuntimeConfig.Recipe;
	Recipe.Settings.ContinentalAmplitude = 0;
	Recipe.Settings.MountainAmplitude = 0;
	Recipe.Settings.DetailAmplitude = 0;
	Recipe.Settings.SeaLevel = -64;
	Recipe.Settings.RiverSourceAccumulation = MAX_int32;
	RuntimeConfig.Recipe = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe));
	const TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config =
		MakeShared<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe>(MoveTemp(RuntimeConfig));
	const TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	FVoxelGenerationQuery Query;
	FString Error;
	if (!TestTrue(TEXT("Create surface query"), FVoxelGenerationQuery::Create(Config, Cache, Query, Error)))
	{
		return false;
	}

	const FVoxelGenerationPipeline Generator(Config, Cache);
	int32 Checked = 0;
	for (const FIntPoint XY : { FIntPoint(0, 0), FIntPoint(16, 16), FIntPoint(-16, -16) })
	{
		FVoxelSurfaceCandidate Candidate;
		if (!TestTrue(TEXT("Resolve candidate"), Query.ResolveSurfaceCandidate(XY.X, XY.Y, Candidate, Error)))
		{
			return false;
		}
		if (!Candidate.bValid || Candidate.bRiver || Candidate.bLake || Candidate.bOcean)
		{
			continue;
		}
		FVoxelBlockState Support;
		FVoxelBlockState Above;
		FVoxelColumnSample Column;
		if (!TestTrue(TEXT("Sample ground column"), Query.SampleEnvironmentColumn(XY.X, XY.Y, Column, Error)))
		{
			return false;
		}
		TestEqual(TEXT("Candidate and coarse terrain share the highest solid cell"), Candidate.GroundZ, Column.SurfaceZ);
		if (!TestTrue(TEXT("Sample support"), Generator.SampleBlock(FIntVector(XY.X, XY.Y, Candidate.GroundZ), Support, Error)) ||
			!TestTrue(TEXT("Sample space above support"), Generator.SampleBlock(FIntVector(XY.X, XY.Y, Candidate.GroundZ + 1), Above, Error)))
		{
			return false;
		}
		TestFalse(TEXT("Spawn support is terrain rather than air"), Support.IsAir());
		TestTrue(TEXT("Space directly above natural terrain is air"), Above.IsAir());
		++Checked;
	}
	TestTrue(TEXT("Checked at least one dry surface"), Checked > 0);
	return true;
}

#endif
