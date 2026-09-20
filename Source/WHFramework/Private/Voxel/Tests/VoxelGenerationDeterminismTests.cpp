#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
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

#endif
