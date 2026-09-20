#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/Caves/VoxelCaveGenerator.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelCaveSegmentCarvingTest,
	"WHFramework.Voxel.Cave.SegmentCarving",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCaveSegmentCarvingTest::RunTest(const FString& InParameters)
{
	(void)InParameters;

	FVoxelCaveSegment Segment;
	Segment.Start = FIntVector(0, 0, 10);
	Segment.End = FIntVector(10, 0, 10);
	Segment.Radius = 2;

	FVoxelCavePlan Plan;
	Plan.Segments.Add(Segment);
	Plan.Finalize();

	TestTrue(TEXT("Segment start is carved"), Plan.Carves(FIntVector(0, 0, 10)));
	TestTrue(TEXT("Segment middle is carved"), Plan.Carves(FIntVector(5, 0, 10)));
	TestTrue(TEXT("Segment end is carved"), Plan.Carves(FIntVector(10, 0, 10)));
	TestFalse(TEXT("Outside segment radius is not carved"), Plan.Carves(FIntVector(5, 3, 10)));
	TestFalse(TEXT("Capsule caves do not protect an artificial floor"), Plan.ProtectsFloor(FIntVector(5, 0, 7)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelCaveRouteTest,
	"WHFramework.Voxel.Cave.Route",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCaveRouteTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config =
		VoxelTest::MakeGenerationConfig();
	const TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Recipe = Config->Recipe.ToSharedRef();
	const FVoxelCaveGenerator Generator(Recipe);
	auto ColumnSampler = [](const FIntVector& InPosition, FVoxelColumnSample& OutColumn)
	{
		(void)InPosition;
		OutColumn.SurfaceZ = 96;
		return true;
	};

	const FVoxelGenerationBounds BoundsA{ FIntVector(0, 0, -64), FIntVector(256, 256, 128) };
	const FVoxelGenerationBounds BoundsB{ FIntVector(1, 1, -64), FIntVector(257, 257, 128) };
	FVoxelCavePlan PlanA;
	FVoxelCavePlan PlanARepeat;
	FVoxelCavePlan PlanB;
	FString Error;
	TestTrue(TEXT("First cave plan builds"), Generator.BuildPlan(BoundsA, ColumnSampler, PlanA, Error));
	TestTrue(TEXT("Repeated cave plan builds"), Generator.BuildPlan(BoundsA, ColumnSampler, PlanARepeat, Error));
	TestTrue(TEXT("Overlapping cave plan builds"), Generator.BuildPlan(BoundsB, ColumnSampler, PlanB, Error));
	TestTrue(TEXT("Fixed seed produces cave segments"), !PlanA.Segments.IsEmpty());
	TestEqual(TEXT("Repeated build has the same segment count"), PlanARepeat.Segments.Num(), PlanA.Segments.Num());

	for (int32 Index = 0; Index < PlanA.Segments.Num() && Index < PlanARepeat.Segments.Num(); ++Index)
	{
		const FVoxelCaveSegment& First = PlanA.Segments[Index];
		const FVoxelCaveSegment& Repeated = PlanARepeat.Segments[Index];
		TestEqual(TEXT("Repeated segment start is deterministic"), Repeated.Start, First.Start);
		TestEqual(TEXT("Repeated segment end is deterministic"), Repeated.End, First.End);
		TestEqual(TEXT("Repeated segment radius is deterministic"), Repeated.Radius, First.Radius);
	}

	bool bFoundSharedSegment = false;
	for (const FVoxelCaveSegment& First : PlanA.Segments)
	{
		for (const FVoxelCaveSegment& Second : PlanB.Segments)
		{
			if (First.Start == Second.Start &&
				First.End == Second.End &&
				First.Radius == Second.Radius)
			{
				bFoundSharedSegment = true;
				break;
			}
		}
		if (bFoundSharedSegment)
		{
			break;
		}
	}
	TestTrue(TEXT("Overlapping cave queries retain shared deterministic segments"), bFoundSharedSegment);
	return true;
}

#endif
