#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/Caves/VoxelCaveGenerator.h"
#include "Voxel/Generation/Climate/VoxelClimateGenerator.h"
#include "Voxel/Generation/Hydrology/VoxelHydrology.h"
#include "Voxel/Generation/Terrain/VoxelTerrainGenerator.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelCaveFloorProtectionTest,
	"WHFramework.Voxel.Cave.FloorProtection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCaveFloorProtectionTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelCaveRoute Route;
	FVoxelCaveRouteSection Section;
	Section.Center = FIntVector::ZeroValue;
	Section.Radius = 3;
	Section.FloorZ = -2;
	Section.CeilingZ = 3;
	Route.Sections.Add(Section);
	TestTrue(TEXT("Interior is carved"), Route.Carves(FIntVector(0, 0, 0)));
	TestTrue(TEXT("Route floor is carved interior"), Route.Carves(FIntVector(0, 0, -2)));
	TestFalse(TEXT("Protected support floor is not carved"), Route.Carves(FIntVector(0, 0, -3)));
	TestTrue(TEXT("Support floor is protected"), Route.ProtectsFloor(FIntVector(0, 0, -3)));
	TestFalse(TEXT("Cell below support floor is not protected"), Route.ProtectsFloor(FIntVector(0, 0, -4)));
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
	const TSharedRef<const FVoxelClimateGenerator, ESPMode::ThreadSafe> Climate =
		MakeShared<const FVoxelClimateGenerator, ESPMode::ThreadSafe>(Recipe);
	const TSharedRef<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> Terrain =
		MakeShared<const FVoxelTerrainGenerator, ESPMode::ThreadSafe>(Recipe, Climate);
	const TSharedRef<const FVoxelHydrologyGenerator, ESPMode::ThreadSafe> Hydrology =
		MakeShared<const FVoxelHydrologyGenerator, ESPMode::ThreadSafe>(Recipe, Terrain);
	const FVoxelCaveGenerator Generator(Recipe, Terrain, Hydrology);
	const FVoxelGenerationBounds BoundsA{ FIntVector(0, 0, -64), FIntVector(256, 256, 128) };
	const FVoxelGenerationBounds BoundsB{ FIntVector(1, 1, -64), FIntVector(257, 257, 128) };
	FVoxelCavePlan PlanA;
	FVoxelCavePlan PlanB;
	FString Error;
	TestTrue(TEXT("First cave plan builds"), Generator.BuildPlan(BoundsA, PlanA, Error));
	TestTrue(TEXT("Overlapping cave plan builds"), Generator.BuildPlan(BoundsB, PlanB, Error));
	TestTrue(TEXT("Fixed seed produces cave routes"), !PlanA.Routes.IsEmpty());

	TSet<FVoxelStableId> SecondIds;
	for (const FVoxelCaveRoute& Route : PlanB.Routes)
	{
		SecondIds.Add(Route.Id);
	}
	int32 SharedRoutes = 0;
	for (const FVoxelCaveRoute& Route : PlanA.Routes)
	{
		if (SecondIds.Contains(Route.Id))
		{
			++SharedRoutes;
		}
		TestTrue(TEXT("Cave route has at least two sections"), Route.Sections.Num() >= 2);
		for (const FVoxelCaveRouteSection& Section : Route.Sections)
		{
			TestTrue(TEXT("Cave route has walkable headroom"), Section.CeilingZ - Section.FloorZ >= 2);
			TestTrue(
				TEXT("Cave route protects its support floor"),
				Route.ProtectsFloor(FIntVector(Section.Center.X, Section.Center.Y, Section.FloorZ - 1)));
			TestFalse(
				TEXT("Protected support floor is not carved"),
				Route.Carves(FIntVector(Section.Center.X, Section.Center.Y, Section.FloorZ - 1)));
		}
	}
	TestTrue(TEXT("Overlapping cave queries retain stable route ids"), SharedRoutes > 0);
	return true;
}

#endif
