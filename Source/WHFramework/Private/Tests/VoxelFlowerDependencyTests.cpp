#include "Misc/AutomationTest.h"
#include "Voxel/Generation/Ecology/VoxelEcology.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFlowerDependencyRangeTest,
	"WHFramework.Voxel.Generation.Ecology.FlowerDependencyRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFlowerDependencyRangeTest::RunTest(const FString& Parameters)
{
	FVoxelGenerationSettings Settings;
	Settings.Ecology.Tree.CrownRadius = 1;
	Settings.Ecology.Grass.PatchRadius = 2;
	Settings.Ecology.Flower.PatchRadius = 96;
	FVoxelGenerationBounds Bounds;
	Bounds.Min = FIntVector(32, 32, 0);
	Bounds.Max = FIntVector(33, 33, 1);
	FVoxelGenerationPlanKeyRanges Ranges;
	FString Error;
	if (!TestTrue(TEXT("Range calculation succeeds"),
		FVoxelGenerationQuery::GetPlanKeyRanges(Bounds, Settings, Ranges, Error))) return false;
	TestEqual(TEXT("Flower halo crosses negative tiles"), Ranges.EcologyMin, FIntPoint(-2, -2));
	TestEqual(TEXT("Flower halo crosses positive tiles"), Ranges.EcologyMax, FIntPoint(2, 2));
	TestEqual(TEXT("All dependency tiles counted"), Ranges.EcologyCount, 25);
	Settings.Ecology.Tree.CrownRadius = 200;
	TestTrue(TEXT("Tree-dominant range succeeds"),
		FVoxelGenerationQuery::GetPlanKeyRanges(Bounds, Settings, Ranges, Error));
	TestEqual(TEXT("Larger tree margin is preserved"), Ranges.EcologyMin, FIntPoint(-3, -3));
	TestEqual(TEXT("Larger tree margin upper bound"), Ranges.EcologyMax, FIntPoint(3, 3));
	Bounds.Min = FIntVector(-33, -33, 0);
	Bounds.Max = FIntVector(-32, -32, 1);
	Settings.Ecology.Tree.CrownRadius = 1;
	TestTrue(TEXT("Negative coordinate range succeeds"),
		FVoxelGenerationQuery::GetPlanKeyRanges(Bounds, Settings, Ranges, Error));
	TestEqual(TEXT("Negative coordinate floor division lower"), Ranges.EcologyMin, FIntPoint(-3, -3));
	TestEqual(TEXT("Negative coordinate floor division upper"), Ranges.EcologyMax, FIntPoint(1, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFlowerCrossTileDependencyTest,
	"WHFramework.Voxel.Generation.Ecology.FlowerCrossTileDependency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFlowerCrossTileDependencyTest::RunTest(const FString& Parameters)
{
	// 仅用于确定性回归，不将此放大半径与密度应用到正式资产。
	auto Recipe = MakeShared<FVoxelGenerationRecipe, ESPMode::ThreadSafe>();
	Recipe->Settings.Ecology.Tree.bEnabled = false;
	Recipe->Settings.Ecology.Grass.bEnabled = false;
	Recipe->Settings.Ecology.Tree.CrownRadius = 0;
	Recipe->Settings.Ecology.Grass.PatchRadius = 0;
	auto& Flower = Recipe->Settings.Ecology.Flower;
	Flower.bEnabled = true;
	Flower.Spacing = FVoxelGenerationQuery::EcologyTileSide;
	Flower.PatchRadius = 70;
	Flower.ChancePermille = 1000;
	Flower.DensityPermille = 1000;
	Flower.PatchFillPermille = 1000;
	Recipe->Palette.Air = 0;
	Recipe->Palette.Grass = 2;
	Recipe->Palette.Sand = 3;
	Recipe->Ecology.Flowers.Add({4, 1});
	Recipe->Biomes.AddDefaulted();
	Recipe->Biomes[0].Ecology.bAllowFlowers = true;
	Recipe->Biomes[0].Ecology.FlowerDensityScalePermille = 1000;
	const TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Frozen = Recipe;
	const FVoxelEcologyGenerator Generator(Frozen);
	auto Column = [](const FIntVector&, FVoxelColumnSample& Out)
	{
		Out = FVoxelColumnSample();
		Out.BiomeIndex = 0;
		Out.SurfaceZ = 8;
		Out.SurfaceMaterial = 2;
		return true;
	};
	auto Base = [](const FIntVector& Position, uint32& Out)
	{
		Out = Position.Z <= 8 ? 2u : 0u;
		return true;
	};
	for (const int32 Owner : {0, -1})
	{
		FVoxelGenerationBounds Bounds;
		Bounds.Min = FIntVector(Owner * 64, Owner * 64, -64);
		Bounds.Max = Bounds.Min + FIntVector(64, 64, 128);
		FVoxelEcologyPlan Plan;
		FString Error;
		if (!TestTrue(TEXT("Synthetic flower plan builds"),
			Generator.BuildPlan(Bounds, Column, Base, Plan, Error))) return false;
		if (!TestTrue(TEXT("Plan has actual flower writes"), Plan.FlowerWrites > 0 && !Plan.Writes.IsEmpty())) return false;
		// 半径为七十时，首尾写入跨越所属瓦片的相反边界。
		for (const int32 Index : {0, Plan.Writes.Num() - 1})
		{
			const FIntVector Point = Plan.Writes[Index].Position;
			TestTrue(TEXT("Flower write crosses its owning tile boundary"),
				Point.X < Bounds.Min.X || Point.X >= Bounds.Max.X);
			FVoxelGenerationBounds PointBounds;
			PointBounds.Min = Point;
			PointBounds.Max = Point + FIntVector(1);
			FVoxelGenerationPlanKeyRanges Ranges;
			if (!TestTrue(TEXT("Cross-tile consumer ranges succeed"),
				FVoxelGenerationQuery::GetPlanKeyRanges(PointBounds, Recipe->Settings, Ranges, Error))) return false;
			TestTrue(TEXT("Consumer includes source flower tile in X"),
				Ranges.EcologyMin.X <= Owner && Ranges.EcologyMax.X >= Owner);
			TestTrue(TEXT("Consumer includes source flower tile in Y"),
				Ranges.EcologyMin.Y <= Owner && Ranges.EcologyMax.Y >= Owner);
			uint32 Value = 0;
			TestTrue(TEXT("Owner plan resolves cross-tile write"), Plan.Sample(Point, Value));
			TestEqual(TEXT("Resolved cross-tile flower symbol"), Value, 4u);
		}
	}
	return true;
}

#endif
