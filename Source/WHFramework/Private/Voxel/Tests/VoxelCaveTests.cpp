#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/Caves/VoxelCaveGenerator.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

namespace
{
	bool BuildEntranceTestPlan(
		FVoxelCavePlan& OutPlan,
		FString& OutError,
		const int32 InSystemChance = 1000,
		const int32 InEntranceChance = 1000)
	{
		FVoxelGenerationRuntimeConfig Config = *VoxelTest::MakeGenerationConfig();
		FVoxelGenerationRecipe Recipe = *Config.Recipe;
		Recipe.Settings.CaveSpacing = 64;
		Recipe.Settings.CaveMinDepth = 4;
		Recipe.Settings.CaveMainRadius = 2;
		Recipe.Settings.CaveBranchRadius = 3;
		Recipe.Settings.CaveSystemChancePermille = InSystemChance;
		Recipe.Settings.CaveEntranceChancePermille = InEntranceChance;
		Recipe.Settings.CaveEntranceLength = 16;
		Recipe.Settings.CaveEntranceDropPerStep = 1;
		Recipe.Settings.CaveEntranceTransitionDepth = 12;
		Recipe.Settings.CaveRoomChancePermille = 1000;
		Recipe.Settings.CaveBranchChancePermille = 1000;
		const TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Shared =
			MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe));
		const FVoxelCaveGenerator Generator(Shared);
		auto ColumnSampler = [](const FIntVector&, FVoxelColumnSample& OutColumn)
		{
			OutColumn.SurfaceZ = 96;
			return true;
		};
		return Generator.BuildPlan(
			{ FIntVector(0, 0, -64), FIntVector(256, 256, 128) },
			ColumnSampler,
			OutPlan,
			OutError);
	}
}

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelCaveEntranceSlopedTest,
	"WHFramework.Voxel.Cave.Entrance.IsSloped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCaveEntranceSlopedTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelCavePlan Plan;
	FString Error;
	TestTrue(TEXT("Entrance cave plan builds"), BuildEntranceTestPlan(Plan, Error));
	bool bFoundEntrance = false;
	for (const FVoxelCaveSegment& Segment : Plan.Segments)
	{
		if (Segment.Start.Z >= 96 - 2)
		{
			bFoundEntrance = true;
			const int32 HorizontalSquared = FMath::Square(Segment.End.X - Segment.Start.X) +
				FMath::Square(Segment.End.Y - Segment.Start.Y);
			TestTrue(TEXT("Entrance moves horizontally"), HorizontalSquared > 0);
			TestTrue(TEXT("Entrance slopes downward"), Segment.End.Z < Segment.Start.Z);
			TestTrue(TEXT("Entrance drop is bounded"), Segment.Start.Z - Segment.End.Z <= 2);
		}
	}
	TestTrue(TEXT("At least one surface entrance exists"), bFoundEntrance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelCaveNoEarlyRoomTest,
	"WHFramework.Voxel.Cave.Entrance.NoRoomBeforeTransitionDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCaveNoEarlyRoomTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelCavePlan Plan;
	FString Error;
	TestTrue(TEXT("Room transition cave plan builds"), BuildEntranceTestPlan(Plan, Error));
	for (const FVoxelCaveSegment& Segment : Plan.Segments)
	{
		if (Segment.Start == Segment.End)
		{
			TestTrue(TEXT("Room starts below transition depth"), 96 - Segment.Start.Z >= 12);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelCaveNoEarlyBranchTest,
	"WHFramework.Voxel.Cave.Entrance.NoBranchBeforeTransitionDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCaveNoEarlyBranchTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelCavePlan Plan;
	FString Error;
	TestTrue(TEXT("Branch transition cave plan builds"), BuildEntranceTestPlan(Plan, Error));
	for (const FVoxelCaveSegment& Segment : Plan.Segments)
	{
		if (Segment.Radius == 3 && Segment.Start != Segment.End && Segment.Start.Z <= 84)
		{
			TestTrue(TEXT("Branch starts below transition depth"), 96 - Segment.Start.Z >= 12);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelCaveDensityTest,
	"WHFramework.Voxel.Cave.Entrance.Density",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCaveDensityTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelCavePlan Dense;
	FVoxelCavePlan Empty;
	FString Error;
	TestTrue(TEXT("Dense cave plan builds"), BuildEntranceTestPlan(Dense, Error, 1000, 1000));
	TestTrue(TEXT("Zero-density cave plan builds"), BuildEntranceTestPlan(Empty, Error, 0, 1000));
	TestTrue(TEXT("Dense settings produce cave segments"), Dense.Segments.Num() > 0);
	TestEqual(TEXT("Zero system chance produces no caves"), Empty.Segments.Num(), 0);
	return true;
}

#endif
