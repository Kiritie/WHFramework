#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Voxel/Generation/Ecology/VoxelEcology.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"

namespace
{
	FVoxelGenerationRecipe MakeEcologyRecipe()
	{
		FVoxelGenerationRecipe Recipe;
		Recipe.Settings.Seed = 12345;
		Recipe.Settings.Ecology = FVoxelEcologyGenerationSettings();
		Recipe.Palette.Air = 0;
		Recipe.Palette.Stone = 1;
		Recipe.Ecology.TreeTrunk = 2;
		Recipe.Ecology.TreeLeaves = 3;
		Recipe.Ecology.GrassPlant = 4;
		return Recipe;
	}

	bool BuildEcologyPlan(const FVoxelGenerationRecipe& Recipe, FVoxelEcologyPlan& OutPlan, FString& OutError)
	{
		const TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Shared =
			MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(Recipe);
		FVoxelEcologyGenerator Generator(Shared);
		auto SampleColumn = [](const FIntVector& InPosition, FVoxelColumnSample& OutColumn)
		{
			(void)InPosition;
			OutColumn.SurfaceZ = 0;
			OutColumn.DensityHeight = 0;
			OutColumn.SlopePermille = 0;
			OutColumn.Climate.TemperatureQ15 = 0;
			OutColumn.Climate.MoistureQ15 = 12000;
			return true;
		};
		auto SampleBase = [](const FIntVector& InPosition, uint32& OutValue)
		{
			OutValue = InPosition.Z <= 0 ? 1 : 0;
			return true;
		};
		return Generator.BuildPlan(
			{FIntVector(-256, -256, -64), FIntVector(256, 256, 128)},
			SampleColumn,
			SampleBase,
			OutPlan,
			OutError);
	}

	int32 CountSymbol(const FVoxelEcologyPlan& Plan, const uint16 Symbol)
	{
		int32 Count = 0;
		for (const FVoxelEcologyPlanWrite& Write : Plan.Writes)
		{
			if (static_cast<uint16>(Write.Value & 0xffffu) == Symbol)
			{
				++Count;
			}
		}
		return Count;
	}

	bool AreWritesEqual(const TArray<FVoxelEcologyPlanWrite>& A, const TArray<FVoxelEcologyPlanWrite>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].Position != B[Index].Position || A[Index].Value != B[Index].Value ||
				A[Index].Priority != B[Index].Priority || A[Index].OwnerId != B[Index].OwnerId)
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelDefaultEcologyTest,
	"WHFramework.Voxel.Generation.DefaultEcology",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelDefaultEcologyTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const FVoxelGenerationRecipe Recipe = MakeEcologyRecipe();
	FVoxelEcologyPlan Plan;
	FString Error;
	TestTrue(TEXT("Default ecology plan builds without profile features"), BuildEcologyPlan(Recipe, Plan, Error));
	TestTrue(TEXT("Default ecology contains tree writes"),
		CountSymbol(Plan, Recipe.Ecology.TreeTrunk) + CountSymbol(Plan, Recipe.Ecology.TreeLeaves) > 0);
	TestTrue(TEXT("Default ecology contains grass writes"), CountSymbol(Plan, Recipe.Ecology.GrassPlant) > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelEcologyOverrideTest,
	"WHFramework.Voxel.Generation.EcologyOverride",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelEcologyOverrideTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FString Error;

	FVoxelGenerationRecipe Recipe = MakeEcologyRecipe();
	FVoxelEcologyPlan First;
	FVoxelEcologyPlan Second;
	TestTrue(TEXT("First deterministic ecology plan builds"), BuildEcologyPlan(Recipe, First, Error));
	TestTrue(TEXT("Second deterministic ecology plan builds"), BuildEcologyPlan(Recipe, Second, Error));
	TestTrue(TEXT("Same seed and settings produce identical writes"), AreWritesEqual(First.Writes, Second.Writes));

	Recipe.Settings.Ecology.Tree.bEnabled = false;
	FVoxelEcologyPlan NoTrees;
	TestTrue(TEXT("Tree-disabled ecology plan builds"), BuildEcologyPlan(Recipe, NoTrees, Error));
	TestEqual(TEXT("Tree disable removes trunks"), CountSymbol(NoTrees, Recipe.Ecology.TreeTrunk), 0);
	TestEqual(TEXT("Tree disable removes leaves"), CountSymbol(NoTrees, Recipe.Ecology.TreeLeaves), 0);
	TestTrue(TEXT("Tree disable keeps grass"), CountSymbol(NoTrees, Recipe.Ecology.GrassPlant) > 0);

	Recipe = MakeEcologyRecipe();
	Recipe.Settings.Ecology.Grass.bEnabled = false;
	FVoxelEcologyPlan NoGrass;
	TestTrue(TEXT("Grass-disabled ecology plan builds"), BuildEcologyPlan(Recipe, NoGrass, Error));
	TestEqual(TEXT("Grass disable removes grass"), CountSymbol(NoGrass, Recipe.Ecology.GrassPlant), 0);
	TestTrue(TEXT("Grass disable keeps trees"), CountSymbol(NoGrass, Recipe.Ecology.TreeTrunk) > 0);

	Recipe = MakeEcologyRecipe();
	Recipe.Settings.Ecology.Tree.DensityPermille = 0;
	FVoxelEcologyPlan ZeroTreeDensity;
	TestTrue(TEXT("Zero-tree-density ecology plan builds"), BuildEcologyPlan(Recipe, ZeroTreeDensity, Error));
	TestEqual(TEXT("Zero tree density removes trunks"), CountSymbol(ZeroTreeDensity, Recipe.Ecology.TreeTrunk), 0);

	Recipe = MakeEcologyRecipe();
	Recipe.Settings.Ecology.Grass.DensityPermille = 0;
	FVoxelEcologyPlan ZeroGrassDensity;
	TestTrue(TEXT("Zero-grass-density ecology plan builds"), BuildEcologyPlan(Recipe, ZeroGrassDensity, Error));
	TestEqual(TEXT("Zero grass density removes grass"), CountSymbol(ZeroGrassDensity, Recipe.Ecology.GrassPlant), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelEcologySmallTileTest,
	"WHFramework.Voxel.Ecology.SmallTile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelEcologySmallTileTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationRecipe Recipe = MakeEcologyRecipe();
	Recipe.Settings.Ecology.Tree.ChancePermille = 1000;
	Recipe.Settings.Ecology.Tree.DensityPermille = 1000;
	const TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Shared =
		MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(Recipe);
	FVoxelEcologyGenerator Generator(Shared);
	FVoxelEcologyPlan Plan;
	FString Error;
	auto SampleColumn = [](const FIntVector&, FVoxelColumnSample& OutColumn)
	{
		OutColumn.SurfaceZ = 0;
		OutColumn.Climate.MoistureQ15 = 12000;
		return true;
	};
	auto SampleBase = [](const FIntVector& InPosition, uint32& OutValue)
	{
		OutValue = InPosition.Z <= 0 ? 1 : 0;
		return true;
	};
	TestTrue(TEXT("64-cell ecology tile builds"), Generator.BuildPlan(
		{ FIntVector(0, 0, -64), FIntVector(64, 64, 128) }, SampleColumn, SampleBase, Plan, Error));
	TestTrue(TEXT("Small tile evaluates tree candidates"), Plan.TreeCandidates > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelEcologyTreeWritesTest,
	"WHFramework.Voxel.Ecology.TreeWrites",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelEcologyTreeWritesTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelEcologyPlan Plan;
	FString Error;
	TestTrue(TEXT("Tree write plan builds"), BuildEcologyPlan(MakeEcologyRecipe(), Plan, Error));
	TestTrue(TEXT("Accepted trees produce writes"), Plan.TreesAccepted > 0 && Plan.Writes.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelEcologyGrassWritesTest,
	"WHFramework.Voxel.Ecology.GrassWrites",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelEcologyGrassWritesTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelEcologyPlan Plan;
	FString Error;
	TestTrue(TEXT("Grass write plan builds"), BuildEcologyPlan(MakeEcologyRecipe(), Plan, Error));
	TestTrue(TEXT("Grass diagnostics count writes"), Plan.GrassPatchCandidates > 0 && Plan.GrassWrites > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelEcologyHydrologyAwareTest,
	"WHFramework.Voxel.Ecology.HydrologyAware",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelEcologyHydrologyAwareTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationRecipe Recipe = MakeEcologyRecipe();
	Recipe.Settings.Ecology.Tree.bAllowNearWater = false;
	Recipe.Settings.Ecology.Tree.ChancePermille = 1000;
	Recipe.Settings.Ecology.Tree.DensityPermille = 1000;
	Recipe.Settings.Ecology.Grass.bEnabled = false;
	const TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Shared =
		MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(Recipe);
	FVoxelEcologyGenerator Generator(Shared);
	FVoxelEcologyPlan Plan;
	FString Error;
	auto RiverColumn = [](const FIntVector&, FVoxelColumnSample& OutColumn)
	{
		OutColumn.SurfaceZ = 0;
		OutColumn.bRiver = true;
		OutColumn.Climate.MoistureQ15 = 12000;
		return true;
	};
	auto SampleBase = [](const FIntVector& InPosition, uint32& OutValue)
	{
		OutValue = InPosition.Z <= 0 ? 1 : 0;
		return true;
	};
	TestTrue(TEXT("River ecology plan builds"), Generator.BuildPlan(
		{ FIntVector(0, 0, -64), FIntVector(64, 64, 128) }, RiverColumn, SampleBase, Plan, Error));
	TestEqual(TEXT("Trees are rejected near river hydrology"), Plan.TreesAccepted, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelEcologySparseGrassTest,
	"WHFramework.Voxel.Ecology.SparseGrass", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelEcologySparseGrassTest::RunTest(const FString& Parameters)
{
	FVoxelGenerationRecipe Recipe = MakeEcologyRecipe();
	Recipe.Settings.Ecology.Tree.bEnabled = false;
	FVoxelEcologyPlan Dense;
	FVoxelEcologyPlan Sparse;
	FString Error;
	if (!BuildEcologyPlan(Recipe, Dense, Error)) return false;
	Recipe.Settings.Ecology.Grass.DensityPermille = 600;
	Recipe.Settings.Ecology.Grass.PatchFillPermille = 300;
	if (!BuildEcologyPlan(Recipe, Sparse, Error)) return false;
	TestTrue(TEXT("Sparse grass keeps visible patches"), Sparse.GrassWrites > 0);
	TestTrue(TEXT("Sparse settings remove more than half the grass geometry"), Sparse.GrassWrites * 2 < Dense.GrassWrites);
	AddInfo(FString::Printf(TEXT("Grass writes: dense=%d sparse=%d retained=%.1f%%"),
		Dense.GrassWrites, Sparse.GrassWrites, 100.0 * Sparse.GrassWrites / FMath::Max(1, Dense.GrassWrites)));
	return true;
}

#endif
