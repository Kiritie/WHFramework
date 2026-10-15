#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/Aquifer/VoxelAquiferGenerator.h"
#include "Voxel/Generation/Climate/VoxelClimateGenerator.h"
#include "Voxel/Generation/Ecology/VoxelNaturalDecoration.h"
#include "Voxel/Generation/Hydrology/VoxelHydrology.h"
#include "Voxel/Generation/Surface/VoxelSurfaceGenerator.h"
#include "Voxel/Generation/Terrain/VoxelTerrainGenerator.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyWorldBoundsTest,
	"WHFramework.Voxel.Generation.WorldBounds.NativeHydrology",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyWorldBoundsTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationRecipe MutableRecipe = *VoxelTest::MakeGenerationConfig()->Recipe;
	MutableRecipe.Settings.HydrologyCellSize = 3;
	MutableRecipe.Settings.HydrologyRegionSide = 8;
	MutableRecipe.Settings.RiverSourceSpacing = 4;
	MutableRecipe.Settings.MaxZ = 4096;
	MutableRecipe.Settings.SeaLevel = MutableRecipe.Settings.MaxZ - 1;
	const auto Recipe = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(MutableRecipe));
	const auto Climate = MakeShared<const FVoxelClimateGenerator, ESPMode::ThreadSafe>(Recipe);
	const auto Terrain = MakeShared<const FVoxelTerrainGenerator, ESPMode::ThreadSafe>(Recipe, Climate);
	const FVoxelHydrologyGenerator Generator(Recipe, Terrain);
	FString Error;
	for (const int32 Coordinate : {MIN_int32, MAX_int32 - 1})
	{
		const auto Key = FVoxelGenerationQuery::HydrologyTopologyKeyForVoxel(Coordinate, Coordinate, Recipe->Settings);
		FVoxelHydrologyPlan Plan;
		if (!TestTrue(TEXT("Native hydrology builds at both world edges"), Generator.BuildTopology(Key, Plan, Error))) return false;
		const int64 CoreOrigin = int64(Key.Coordinate.X) * Recipe->Settings.HydrologyRegionSide * Plan.Grid.CellSize;
		TestEqual(TEXT("Hydrology core uses voxel units"), Plan.CoreMin.X,
			int32(FMath::Max(CoreOrigin, VoxelGeneration::WorldCellMin)));
		TestEqual(TEXT("Hydrology core excludes the sentinel"), Plan.CoreMax.X,
			int32(FMath::Min(CoreOrigin + int64(Recipe->Settings.HydrologyRegionSide) * Plan.Grid.CellSize,
				VoxelGeneration::WorldCellMaxExclusive)));
		TSet<FIntPoint> Samples;
		for (uint32 Index = 0; Index < uint32(Plan.Grid.GroundPlane.Num()); ++Index)
		{
			FIntPoint Sample;
			if (!TestTrue(TEXT("Every native grid cell has a valid physical sample"), Plan.Grid.TryToWorldVoxelXY(Index, Sample))) return false;
			TestTrue(TEXT("Grid sampling never wraps outside the domain"), VoxelGeneration::IsWorldCell(Sample.X) && VoxelGeneration::IsWorldCell(Sample.Y));
			TestFalse(TEXT("Clipped halo never duplicates a physical cell"), Samples.Contains(Sample));
			Samples.Add(Sample);
			TestEqual(TEXT("Grid contains the terrain value at its physical sample"), Plan.Grid.GroundPlane[Index], Terrain->SampleMacro(Sample.X, Sample.Y).SurfaceZ);
			TestTrue(TEXT("Wet world-edge fixture has positive macro water depth"), Plan.Grid.GroundPlane[Index] < Recipe->Settings.SeaLevel);
		}
		FVoxelHydrologyInfluence Influence;
		TestTrue(TEXT("World edge is consumable by the native plan"), Plan.Sample(Coordinate, Coordinate, 32, Influence));
		TestTrue(TEXT("Ocean remains present at the world edge"), Influence.bOcean);
	}
	FVoxelHydrologyPlan Unchanged;
	Unchanged.Key = {FIntPoint(3, 4)};
	TestFalse(TEXT("An out-of-domain region is rejected"), Generator.BuildTopology({FIntPoint(MAX_int32, 0)}, Unchanged, Error));
	TestEqual(TEXT("Invalid region preserves the caller's plan"), Unchanged.Key.Coordinate, FIntPoint(3, 4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyPartialCellTest,
	"WHFramework.Voxel.Generation.WorldBounds.HydrologyPartialCell",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyPartialCellTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelHydrologyPlan Plan;
	Plan.Grid.CellSize = 3;
	Plan.Grid.Width = 3;
	Plan.Grid.Height = 3;
	const int32 FirstCell = int32(VoxelGeneration::FloorDivideWide(MIN_int32, 3));
	Plan.Grid.WorldMinCell = FIntPoint(FirstCell, FirstCell);
	FVoxelLakePlan Lake;
	Lake.WaterZ = 10;
	FIntPoint PhysicalOrigin;
	if (!TestTrue(TEXT("A partially intersecting negative cell has a sample"), Plan.Grid.TryToWorldVoxelXY(0, PhysicalOrigin))) return false;
	TestEqual(TEXT("The partial cell uses its unique physical origin"), PhysicalOrigin, FIntPoint(MIN_int32, MIN_int32));
	Lake.Cells.Add(PhysicalOrigin);
	Plan.Lakes.Add(MoveTemp(Lake));
	Plan.Finalize();
	FVoxelHydrologyInfluence Influence;
	TestTrue(TEXT("Lake lookup uses the producer's physical cell identity"), Plan.Sample(MIN_int32, MIN_int32, 20, Influence));
	TestTrue(TEXT("Partial-cell lake stays addressable"), Influence.bLake);
	TestEqual(TEXT("Partial-cell lake retains its water plane"), Influence.SurfaceWaterZ, 10);
	Influence.SurfaceWaterZ = 71;
	TestFalse(TEXT("A distant valid cell cannot wrap into this small grid"), Plan.Sample(MAX_int32 - 1, MAX_int32 - 1, 20, Influence));
	TestEqual(TEXT("Failed sampling preserves output"), Influence.SurfaceWaterZ, 71);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyWideRiverTest,
	"WHFramework.Voxel.Generation.WorldBounds.HydrologyWideRiver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyWideRiverTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelHydrologyPlan Plan;
	Plan.CoreMin = FIntPoint(MIN_int32, MIN_int32);
	Plan.CoreMax = FIntPoint(MAX_int32, MAX_int32);
	Plan.Grid.CellSize = MAX_int32;
	Plan.Grid.Width = 3;
	Plan.Grid.Height = 3;
	Plan.Grid.WorldMinCell = FIntPoint(-2, -2);
	Plan.RiverShapeSmoothingPasses = 2;
	Plan.RiverMeanderStrength = 128;
	FVoxelRiverRoute Route;
	for (const int32 X : {MIN_int32, MAX_int32 - 1})
	{
		FVoxelRiverRoutePoint Point;
		Point.Position = FIntPoint(X, 0);
		Point.WaterZ = 10;
		Point.BedHalfWidth = 4;
		Point.WaterHalfWidth = 4;
		Point.BankWidth = 4;
		Point.ShoreWidth = 8;
		Point.Depth = 2;
		Route.Points.Add(Point);
	}
	Plan.Rivers.Add(MoveTemp(Route));
	Plan.Finalize();
	FString Error;
	TestTrue(TEXT("Full-span smoothing and meander produce a valid route"), Plan.ValidateRiverRoutes(Error));
	TestEqual(TEXT("The first stable anchor is preserved"), Plan.Rivers[0].MeanderPoints[0].Position.X, MIN_int32);
	TestEqual(TEXT("The final stable anchor is preserved"), Plan.Rivers[0].MeanderPoints.Last().Position.X, MAX_int32 - 1);
	FVoxelHydrologyInfluence Influence;
	const FIntPoint RiverSample = Plan.Rivers[0].MeanderPoints[Plan.Rivers[0].MeanderPoints.Num() / 2].Position;
	TestTrue(TEXT("Wide-segment projection remains consumable"), Plan.Sample(RiverSample.X, RiverSample.Y, 10, Influence));
	TestTrue(TEXT("Wide-segment center retains river water"), Influence.bRiver);
	const FIntPoint First = Plan.Rivers[0].MeanderPoints[0].Position;
	const FIntPoint Second = Plan.Rivers[0].MeanderPoints[1].Position;
	const FIntPoint Interior(int32(int64(First.X) + (int64(Second.X) - First.X) / 3),
		int32(int64(First.Y) + (int64(Second.Y) - First.Y) / 3));
	TestTrue(TEXT("A real interior point exercises a nonzero wide projection"), Interior != First && Interior != Second);
	Influence = FVoxelHydrologyInfluence();
	TestTrue(TEXT("Full-precision wide projection reaches the interior channel"), Plan.Sample(Interior.X, Interior.Y, 10, Influence));
	TestTrue(TEXT("Q16 attribute interpolation cannot move the channel away from its interior"), Influence.bRiver);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelGenerationQueryWorldBoundsTest,
	"WHFramework.Voxel.Generation.WorldBounds.QueryGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelGenerationQueryWorldBoundsTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	FVoxelGenerationQuery Query;
	FString Error;
	if (!TestTrue(TEXT("A ready-only query can be created"), FVoxelGenerationQuery::Create(Config, Cache, Query, Error, true, true, true))) return false;
	FVoxelCavePlanPtr Cave;
	FVoxelStructurePlanPtr Structure;
	FVoxelFeaturePlanPtr Feature;
	FVoxelEcologyPlanPtr Ecology;
	FVoxelHydrologyPlanPtr Hydrology;
	const FVoxelGenerationTileKey InvalidTile{FIntVector(MAX_int32, 0, 0)};
	TestFalse(TEXT("Invalid cave tile is rejected before dependency capture"), Query.EnsureCavePlan(InvalidTile, Cave, Error));
	TestTrue(TEXT("Cave reports invalid identity instead of a missing dependency"), Error.StartsWith(TEXT("Invalid")));
	TestFalse(TEXT("Invalid structure tile is rejected before dependency capture"), Query.EnsureStructurePlan(InvalidTile, Structure, Error));
	TestTrue(TEXT("Structure reports invalid identity"), Error.StartsWith(TEXT("Invalid")));
	TestFalse(TEXT("Invalid feature tile is rejected before dependency capture"), Query.EnsureFeaturePlan(InvalidTile, Feature, Error));
	TestTrue(TEXT("Feature reports invalid identity"), Error.StartsWith(TEXT("Invalid")));
	TestFalse(TEXT("Invalid ecology tile is rejected before dependency capture"), Query.EnsureEcologyPlan({FIntPoint(MAX_int32, 0)}, Ecology, Error));
	TestTrue(TEXT("Ecology reports invalid identity"), Error.StartsWith(TEXT("Invalid")));
	TestFalse(TEXT("Invalid hydrology region is rejected before dependency capture"), Query.EnsureHydrologyPlan({FIntPoint(MAX_int32, 0)}, Hydrology, Error));
	TestTrue(TEXT("Hydrology reports invalid identity"), Error.StartsWith(TEXT("Invalid")));
	FVoxelColumnSample Column;
	Column.SurfaceZ = 93;
	TestFalse(TEXT("The exclusive cell sentinel cannot be sampled"), Query.SampleEnvironmentColumn(MAX_int32, 0, Column, Error));
	TestEqual(TEXT("Invalid column preserves output"), Column.SurfaceZ, 93);
	const FVoxelGenerationPipeline Pipeline(Config, Cache);
	FVoxelBlockState State(1, 0);
	TestFalse(TEXT("Single-block entry rejects a sentinel before computing bounds"), Pipeline.SampleBlock(FIntVector(MAX_int32, 0, 0), State, Error));
	TestTrue(TEXT("Rejected single-block entry preserves output"), State == FVoxelBlockState(1, 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelGenerationGeometryWorldBoundsTest,
	"WHFramework.Voxel.Generation.WorldBounds.GeometryAndDecoration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelGenerationGeometryWorldBoundsTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	TestTrue(TEXT("A full-span capsule contains its center"), VoxelGeneration::IsInsideCapsule(FIntVector::ZeroValue,
		FIntVector(MIN_int32, 0, 0), FIntVector(MAX_int32 - 1, 0, 0), 2));
	TestFalse(TEXT("A full-span capsule rejects a point beyond its radius"), VoxelGeneration::IsInsideCapsule(FIntVector(0, 3, 0),
		FIntVector(MIN_int32, 0, 0), FIntVector(MAX_int32 - 1, 0, 0), 2));
	TestTrue(TEXT("A huge ellipsoid preserves its integer inclusion rule"), VoxelGeneration::IsInsideEllipsoid(
		FIntVector(MAX_int32 - 1, 0, 0), FIntVector::ZeroValue, FIntVector(MAX_int32)));
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Recipe = Config->Recipe.ToSharedRef();
	const auto Climate = MakeShared<const FVoxelClimateGenerator, ESPMode::ThreadSafe>(Recipe);
	const FVoxelTerrainGenerator Terrain(Recipe, Climate);
	FVoxelMacroTerrainSample Macro;
	Macro.DensityHeight = 32;
	TestEqual(TEXT("Deep legal density retains its sign without overflow"), Terrain.SampleDensityQ16(FIntVector(0, 0, MIN_int32), Macro), MAX_int32);
	TestEqual(TEXT("High legal density retains its sign without overflow"), Terrain.SampleDensityQ16(FIntVector(0, 0, MAX_int32 - 1), Macro), MIN_int32);
	FVoxelGenerationRecipe SurfaceRecipe = *Recipe;
	FVoxelSurfaceRuntimeRule Rule;
	Rule.MinDepth = MAX_int32;
	Rule.MaxDepth = MAX_int32;
	Rule.BlockSymbol = SurfaceRecipe.Palette.Grass;
	FVoxelSurfaceRuntimeRuleSet RuleSet;
	RuleSet.Rules.Add(Rule);
	SurfaceRecipe.SurfaceRules.Add(MoveTemp(RuleSet));
	SurfaceRecipe.Biomes[0].SurfaceRuleIndex = SurfaceRecipe.SurfaceRules.Num() - 1;
	const auto WideSurfaceRecipe = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(SurfaceRecipe));
	const FVoxelSurfaceGenerator Surface(WideSurfaceRecipe);
	FVoxelColumnSample DeepColumn;
	DeepColumn.BiomeIndex = 0;
	DeepColumn.SurfaceZ = 32;
	TestEqual(TEXT("Deep surface rules receive the full depth, without saturation or wrap"),
		Surface.ResolveSymbol(FIntVector(0, 0, MIN_int32), DeepColumn, int64(32) - MIN_int32), uint32(Recipe->Palette.Stone));
	FVoxelGenerationRecipe AquiferRecipe = *Recipe;
	AquiferRecipe.Settings.AquiferSpacing = 9;
	AquiferRecipe.Settings.AquiferRadius = 8;
	const auto WideAquiferRecipe = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(AquiferRecipe));
	const FVoxelAquiferGenerator Aquifer(WideAquiferRecipe);
	for (const int32 X : {MIN_int32, MAX_int32 - 1})
	{
		const FVoxelAquiferSample Fluid = Aquifer.Sample(FIntVector(X, 4, 4), DeepColumn, -1);
		TestTrue(TEXT("An aquifer with an out-of-domain center retains its legal residual water"), Fluid.Fluid == EVoxelFluidKind::Water);
		TestTrue(TEXT("Residual aquifer water uses its original local plane"), Fluid.SurfaceZ >= 4 && Fluid.SurfaceZ <= 8);
	}
	FVoxelGenerationRecipe DecorationRecipe = *Recipe;
	DecorationRecipe.Ecology.NaturalDecorations.Add(TEXT("WetRock"), 11);
	FVoxelColumnSample Column;
	Column.SurfaceZ = 32;
	Column.Climate.MoistureQ15 = 1;
	int32 Samples = 0;
	bool bInvalidSample = false;
	FVoxelNaturalDecoration::Resolve(DecorationRecipe, Column, FIntVector(MIN_int32, 0, MIN_int32), DecorationRecipe.Palette.Stone,
		[&](const FIntVector& Cell)
		{
			++Samples;
			bInvalidSample |= !VoxelGeneration::IsWorldCell(Cell.X) || !VoxelGeneration::IsWorldCell(Cell.Y) || !VoxelGeneration::IsWorldCell(Cell.Z);
			return false;
		}, [](const FIntVector&) { return false; });
	TestTrue(TEXT("Native decoration still samples legal neighbors"), Samples > 0);
	TestFalse(TEXT("Native decoration never invokes a sampler on a wrapped neighbor"), bInvalidSample);
	return true;
}

#endif
