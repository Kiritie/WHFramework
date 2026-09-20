#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/Climate/VoxelClimateGenerator.h"
#include "Voxel/Generation/Hydrology/VoxelHydrology.h"
#include "Voxel/Generation/Terrain/VoxelTerrainGenerator.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelHydrologyDrainageTest,
	"WHFramework.Voxel.Hydrology.Drainage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyDrainageTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelDrainageInput Input;
	Input.Width = 3;
	Input.Height = 3;
	Input.GroundPlane = { 9, 8, 7, 8, 7, 6, 7, 6, 5 };
	Input.RainWeight.Init(1, 9);
	Input.Allowed.Init(1, 9);
	Input.Outlets.Add(8, 5);
	FVoxelDrainageResult First;
	FVoxelDrainageResult Second;
	FString Error;
	TestTrue(TEXT("First drainage build"), VoxelHydrology::BuildDrainage(Input, First, Error));
	TestTrue(TEXT("Second drainage build"), VoxelHydrology::BuildDrainage(Input, Second, Error));
	TestEqual(TEXT("All cells settle"), First.SettledOrder.Num(), 9);
	TestTrue(TEXT("Parent selection is deterministic"), First.Parent == Second.Parent);
	TestEqual(TEXT("All runoff reaches outlet"), First.Accumulation[8], uint64(9));
	for (uint32 Source = 0; Source < 9; ++Source)
	{
		TArray<uint32> Path;
		TestTrue(TEXT("Drainage route terminates"), VoxelHydrology::TraceDrainage(Input, First, Source, 16, Path, Error));
		TestEqual(TEXT("Drainage route ends at certified outlet"), Path.Last(), uint32(8));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelHydrologyBasinTest,
	"WHFramework.Voxel.Hydrology.Basin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyBasinTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelBasinInput Input;
	Input.Width = 3;
	Input.Height = 3;
	Input.WaterPlane = 5;
	Input.GroundPlane = { 6, 6, 6, 6, 4, 6, 6, 6, 6 };
	Input.Known.Init(1, 9);
	Input.WaterMask.Init(0, 9);
	Input.WaterMask[4] = 1;
	FVoxelBasinCertificate Certificate;
	FString Error;
	TestTrue(TEXT("Closed basin is certified"), VoxelHydrology::ValidateBasin(Input, Certificate, Error));
	Input.Known[1] = 0;
	TestFalse(TEXT("Unknown shore is rejected"), VoxelHydrology::ValidateBasin(Input, Certificate, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelHydrologyRiverSectionTest,
	"WHFramework.Voxel.Hydrology.RiverSection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyRiverSectionTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelRiverShape Shape;
	Shape.Depth = 3;
	Shape.HalfWidth = 2;
	Shape.BankWidth = 2;
	Shape.ShoreWidth = 4;
	Shape.MaxCutFill = 32;
	FVoxelRiverSection Center;
	FVoxelRiverSection Shore;
	FString Error;
	TestTrue(TEXT("Center river section"), VoxelHydrology::EvaluateRiverSection(12, 10, 0, Shape, Center, Error));
	TestTrue(TEXT("Shore river section"), VoxelHydrology::EvaluateRiverSection(12, 10, 8, Shape, Shore, Error));
	TestTrue(TEXT("Center is wet"), Center.bWet);
	TestEqual(TEXT("Water depth"), Center.WaterPlane - Center.BedPlane, Shape.Depth);
	TestTrue(TEXT("Ground rises toward natural shore"), Shore.GroundPlane >= Center.GroundPlane);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelHydrologySeamTest,
	"WHFramework.Voxel.Hydrology.Seam",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologySeamTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config =
		VoxelTest::MakeGenerationConfig();
	const TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Recipe = Config->Recipe.ToSharedRef();
	const TSharedRef<const FVoxelClimateGenerator, ESPMode::ThreadSafe> Climate =
		MakeShared<const FVoxelClimateGenerator, ESPMode::ThreadSafe>(Recipe);
	const TSharedRef<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> Terrain =
		MakeShared<const FVoxelTerrainGenerator, ESPMode::ThreadSafe>(Recipe, Climate);
	const FVoxelHydrologyGenerator Generator(Recipe, Terrain);
	FVoxelHydrologyPlan PlanA;
	FVoxelHydrologyPlan PlanB;
	FString Error;
	TestTrue(TEXT("Left hydrology region builds"), Generator.BuildPlan({ FIntPoint(0, 0) }, PlanA, Error));
	TestTrue(TEXT("Right hydrology region builds"), Generator.BuildPlan({ FIntPoint(1, 0) }, PlanB, Error));

	int32 SharedCells = 0;
	for (uint32 IndexA = 0; IndexA < static_cast<uint32>(PlanA.Grid.GroundPlane.Num()); ++IndexA)
	{
		const FIntPoint WorldCell = PlanA.Grid.ToWorldHydrologyCell(IndexA);
		const FIntPoint LocalB = WorldCell - PlanB.Grid.WorldMinCell;
		if (LocalB.X < 0 || LocalB.Y < 0 || LocalB.X >= PlanB.Grid.Width || LocalB.Y >= PlanB.Grid.Height)
		{
			continue;
		}
		const uint32 IndexB = PlanB.Grid.ToIndex(LocalB.X, LocalB.Y);
		++SharedCells;
		TestEqual(TEXT("Shared ground plane matches"), PlanA.Grid.GroundPlane[IndexA], PlanB.Grid.GroundPlane[IndexB]);
		TestEqual(TEXT("Shared spill plane matches"), PlanA.Drainage.SpillPlane[IndexA], PlanB.Drainage.SpillPlane[IndexB]);

		auto ParentWorld = [](const FVoxelHydrologyPlan& InPlan, const uint32 InIndex)
		{
			const int32 Parent = InPlan.Drainage.Parent[InIndex];
			return Parent < 0 ? FIntPoint(MAX_int32, MAX_int32) :
				InPlan.Grid.ToWorldHydrologyCell(static_cast<uint32>(Parent));
		};
		TestEqual(TEXT("Shared drainage parent matches"), ParentWorld(PlanA, IndexA), ParentWorld(PlanB, IndexB));
	}
	TestTrue(TEXT("Adjacent plans share a certified halo"), SharedCells > 0);
	return true;
}

#endif
