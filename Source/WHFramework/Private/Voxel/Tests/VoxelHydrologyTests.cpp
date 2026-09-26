#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/Climate/VoxelClimateGenerator.h"
#include "Voxel/Generation/Hydrology/VoxelHydrology.h"
#include "Voxel/Generation/Surface/VoxelSurfaceGenerator.h"
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
		bool bDescended = false;
		for (int32 Index = 1; Index < Path.Num(); ++Index)
		{
			const int32 Upstream = First.SpillPlane[Path[Index - 1]];
			const int32 Downstream = First.SpillPlane[Path[Index]];
			TestTrue(TEXT("Downstream water never rises"), Downstream <= Upstream);
			bDescended |= Downstream < Upstream;
		}
		if (Source == 0)
		{
			TestTrue(TEXT("Gradient basin route descends"), bDescended);
		}
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
	Shape.BedHalfWidth = 2;
	Shape.WaterHalfWidth = 2;
	Shape.BankWidth = 2;
	Shape.ShoreWidth = 4;
	Shape.MaxCutFill = 32;
	FVoxelRiverSection Center;
	FVoxelRiverSection Shore;
	FString Error;
	TestTrue(TEXT("Center river section"), VoxelHydrology::EvaluateRiverSection(12, 10, 0, Shape, Center, Error));
	TestTrue(TEXT("Shore river section"), VoxelHydrology::EvaluateRiverSection(12, 10, 8, Shape, Shore, Error));
	TestTrue(TEXT("Center is wet"), Center.bWet);
	TestEqual(TEXT("Channel bed has an explicit zone"),
		static_cast<uint8>(Center.Zone), static_cast<uint8>(EVoxelRiverSurfaceZone::ChannelBed));
	FVoxelRiverSection Bank;
	TestTrue(TEXT("Bank river section"), VoxelHydrology::EvaluateRiverSection(12, 10, 3, Shape, Bank, Error));
	TestFalse(TEXT("Bank is dry even next to the channel"), Bank.bWet);
	TestEqual(TEXT("Bank has a dry zone"),
		static_cast<uint8>(Bank.Zone), static_cast<uint8>(EVoxelRiverSurfaceZone::DryBank));
	TestEqual(TEXT("Water depth"), Center.WaterPlane - Center.BedPlane, Shape.Depth);
	TestTrue(TEXT("Ground rises toward natural shore"), Shore.GroundPlane >= Center.GroundPlane);
	Shape.MaxCutFill = 1;
	FVoxelRiverSection DeepChannel;
	TestTrue(TEXT("Cut/fill limit cannot erase a wet channel"),
		VoxelHydrology::EvaluateRiverSection(25, 10, 0, Shape, DeepChannel, Error));
	TestTrue(TEXT("Exceeded cut/fill is diagnosed"), DeepChannel.bExceededCutFill);
	TestTrue(TEXT("Exceeded channel remains wet"), DeepChannel.bWet);
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
		const FIntPoint LocalA = PlanA.Grid.ToLocal(IndexA);
		constexpr int32 CertificationMargin = 2;
		if (LocalA.X < CertificationMargin || LocalA.Y < CertificationMargin ||
			LocalA.X >= PlanA.Grid.Width - CertificationMargin ||
			LocalA.Y >= PlanA.Grid.Height - CertificationMargin ||
			LocalB.X < CertificationMargin || LocalB.Y < CertificationMargin ||
			LocalB.X >= PlanB.Grid.Width - CertificationMargin ||
			LocalB.Y >= PlanB.Grid.Height - CertificationMargin)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelHydrologyRiverHeightTest,
	"WHFramework.Voxel.Hydrology.RiverWaterContinuous",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyRiverHeightTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config =
		VoxelTest::MakeGenerationConfig();
	const TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Recipe = Config->Recipe.ToSharedRef();
	const TSharedRef<const FVoxelClimateGenerator, ESPMode::ThreadSafe> Climate =
		MakeShared<const FVoxelClimateGenerator, ESPMode::ThreadSafe>(Recipe);
	const TSharedRef<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> Terrain =
		MakeShared<const FVoxelTerrainGenerator, ESPMode::ThreadSafe>(Recipe, Climate);
	FVoxelHydrologyPlan Plan;
	FString Error;
	TestTrue(TEXT("Hydrology region builds"), FVoxelHydrologyGenerator(Recipe, Terrain).BuildPlan({ FIntPoint(0, 0) }, Plan, Error));
	TestTrue(TEXT("Hydrology region contains river routes"), !Plan.Rivers.IsEmpty());
	bool bHasLocalWaterHeight = false;
	for (const FVoxelRiverRoute& Route : Plan.Rivers)
	{
		for (int32 PointIndex = 0; PointIndex < Route.Points.Num(); ++PointIndex)
		{
			const FVoxelRiverRoutePoint& Point = Route.Points[PointIndex];
			bHasLocalWaterHeight |= Point.WaterZ != Recipe->Settings.SeaLevel + 4;
			if (PointIndex > 0)
			{
				TestTrue(TEXT("Route water never rises downstream"),
					Point.WaterZ <= Route.Points[PointIndex - 1].WaterZ);
			}
		}
	}
	TestTrue(TEXT("River water height is derived from local drainage"), bHasLocalWaterHeight);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelHydrologyHighlandRiverTest,
	"WHFramework.Voxel.Hydrology.HighlandRiver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyHighlandRiverTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config =
		VoxelTest::MakeGenerationConfig(919);
	const TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Recipe = Config->Recipe.ToSharedRef();
	const TSharedRef<const FVoxelClimateGenerator, ESPMode::ThreadSafe> Climate =
		MakeShared<const FVoxelClimateGenerator, ESPMode::ThreadSafe>(Recipe);
	const TSharedRef<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> Terrain =
		MakeShared<const FVoxelTerrainGenerator, ESPMode::ThreadSafe>(Recipe, Climate);
	FVoxelHydrologyPlan Plan;
	FString Error;
	TestTrue(TEXT("Highland hydrology region builds"), FVoxelHydrologyGenerator(Recipe, Terrain).BuildPlan({ FIntPoint(0, 0) }, Plan, Error));
	bool bFoundHighlandPoint = false;
	bool bFoundRoute = false;
	for (const FVoxelRiverRoute& Route : Plan.Rivers)
	{
		for (int32 PointIndex = 0; PointIndex < Route.Points.Num(); ++PointIndex)
		{
			const FVoxelRiverRoutePoint& Point = Route.Points[PointIndex];
			if (Point.WaterZ > Recipe->Settings.SeaLevel + 4)
			{
				bFoundHighlandPoint = true;
				TestNotEqual(TEXT("Highland river is not clamped to global river level"), Point.WaterZ, Recipe->Settings.SeaLevel + 4);
			}
			if (PointIndex > 0)
			{
				bFoundRoute = true;
				TestTrue(TEXT("Highland route water never rises downstream"),
					Point.WaterZ <= Route.Points[PointIndex - 1].WaterZ);
			}
		}
	}
	TestTrue(TEXT("A river route contains highland water"), bFoundHighlandPoint);
	TestTrue(TEXT("Highland region contains a route segment"), bFoundRoute);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelHydrologyInfluenceContextTest,
	"WHFramework.Voxel.Hydrology.InfluenceContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyInfluenceContextTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelHydrologyPlan Plan;
	Plan.Grid.WorldMinCell = FIntPoint(-2, -2);
	Plan.Grid.Width = 8;
	Plan.Grid.Height = 8;
	Plan.Grid.CellSize = 32;
	FVoxelRiverRoute Route;
	Route.Id.High = 1;
	Route.Id.Low = 2;
	FVoxelRiverRoutePoint A;
	A.Position = FIntPoint(0, 0);
	A.WaterZ = 10;
	A.BedHalfWidth = 4;
	A.WaterHalfWidth = 4;
	A.Depth = 4;
	FVoxelRiverRoutePoint B = A;
	B.Position = FIntPoint(32, 0);
	Route.Points = { A, B };
	Plan.Rivers.Add(Route);
	Plan.Finalize();

	FVoxelHydrologyInfluence Channel;
	if (!TestTrue(TEXT("Channel influence resolves"),
		Plan.Sample(16, 0, 12, Channel)))
	{
		return false;
	}
	TestTrue(TEXT("Channel carries the route identity"), Channel.RiverId == Route.Id);
	TestEqual(TEXT("Channel centerline distance"), Channel.RiverDistanceCells, 0);
	TestTrue(TEXT("Channel is wet"), Channel.bRiver);
	TestEqual(TEXT("Channel floodplain strength"), Channel.FloodplainStrengthQ15, 32767);

	FVoxelHydrologyInfluence Shore;
	if (!TestTrue(TEXT("Dry shore influence resolves"),
		Plan.Sample(16, 10, 12, Shore)))
	{
		return false;
	}
	TestTrue(TEXT("Dry shore keeps river identity"), Shore.RiverId == Route.Id);
	TestFalse(TEXT("Dry shore is not river water"), Shore.bRiver);
	TestTrue(TEXT("Dry shore has floodplain influence"),
		Shore.FloodplainStrengthQ15 > 0 && Shore.FloodplainStrengthQ15 < 32767);
	TestTrue(TEXT("Dry shore has bank distance"), Shore.BankDistanceCells > 0);

	FVoxelHydrologyInfluence Far;
	TestFalse(TEXT("Far cell has no local river influence"),
		Plan.Sample(16, 128, 12, Far));
	TestEqual(TEXT("Far cell has no river distance"), Far.RiverDistanceCells, MAX_int32);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelHydrologyShapeRouteTest,
	"WHFramework.Voxel.Hydrology.ShapeRouteRemovesHardCorner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyShapeRouteTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelHydrologyPlan Plan;
	Plan.Grid.WorldMinCell = FIntPoint(-2, -2);
	Plan.Grid.Width = 8;
	Plan.Grid.Height = 8;
	Plan.Grid.CellSize = 32;
	Plan.RiverShapeSmoothingPasses = 1;
	FVoxelRiverRoute Route;
	for (const FIntPoint Position : {FIntPoint(0, 0), FIntPoint(32, 0),
		FIntPoint(32, 32)})
	{
		FVoxelRiverRoutePoint& Point = Route.Points.AddDefaulted_GetRef();
		Point.Position = Position;
		Point.WaterZ = 10;
		Point.BedHalfWidth = 3;
		Point.WaterHalfWidth = 3;
		Point.Depth = 4;
	}
	Plan.Rivers.Add(MoveTemp(Route));
	Plan.Finalize();
	const TArray<FVoxelRiverShapePoint>& Shape = Plan.Rivers[0].ShapePoints;
	if (!TestTrue(TEXT("Shape route has intermediate corners"), Shape.Num() > 3))
	{
		return false;
	}
	TestTrue(TEXT("Headwater anchor is unchanged"), Shape[0].Position == FIntPoint(0, 0));
	TestTrue(TEXT("Confluence anchor is unchanged"), Shape.Last().Position == FIntPoint(32, 32));
	for (int32 Index = 1; Index < Shape.Num(); ++Index)
	{
		TestFalse(TEXT("Raw axis-aligned right angle is absent"),
			Shape[Index].Position == FIntPoint(32, 0));
	}
	FVoxelHydrologyInfluence Influence;
	TestTrue(TEXT("Plan samples the smoothed bend"),
		Plan.Sample(28, 4, 12, Influence));
	TestTrue(TEXT("Shape bend has river influence"),
		Influence.RiverZone != EVoxelRiverSurfaceZone::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyMeanderRouteTest,
	"WHFramework.Voxel.Hydrology.MeanderRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyMeanderRouteTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	auto MakePlan = [](const int32 Seed)
	{
		FVoxelHydrologyPlan Plan;
		Plan.Grid.WorldMinCell = FIntPoint(-16, -16);
		Plan.Grid.Width = 64;
		Plan.Grid.Height = 64;
		Plan.Grid.CellSize = 32;
		Plan.RiverSeed = Seed;
		Plan.RiverShapeSmoothingPasses = 1;
		Plan.RiverMeanderStrength = 24;
		Plan.RiverMeanderFrequency = 96;
		Plan.RiverMeanderOctaves = 2;
		FVoxelRiverRoute& Route = Plan.Rivers.AddDefaulted_GetRef();
		Route.Id.High = 17;
		Route.Id.Low = 29;
		Route.bContinuation = true;
		for (int32 Index = 0; Index < 9; ++Index)
		{
			FVoxelRiverRoutePoint& Point = Route.Points.AddDefaulted_GetRef();
			Point.Position = FIntPoint(Index * 32, Index % 3 == 1 ? 8 : 0);
			Point.WaterZ = 20 - Index;
			Point.BedHalfWidth = 5;
			Point.WaterHalfWidth = 5;
			Point.BankWidth = 5;
			Point.ShoreWidth = 8;
			Point.Depth = 4;
		}
		Plan.Finalize();
		return Plan;
	};
	const FVoxelHydrologyPlan First = MakePlan(71);
	const FVoxelHydrologyPlan Again = MakePlan(71);
	const FVoxelHydrologyPlan Different = MakePlan(87);
	const FVoxelRiverRoute& Route = First.Rivers[0];
	const TArray<FVoxelRiverMeanderPoint>& Points = Route.MeanderPoints;
	TestTrue(TEXT("Meander retains continuation identity"), Route.bContinuation);
	TestTrue(TEXT("Shape route has a stable upstream anchor"),
		Points[0].Position == Route.Points[0].Position);
	TestTrue(TEXT("Shape route has a stable downstream anchor"),
		Points.Last().Position == Route.Points.Last().Position);
	TestEqual(TEXT("Same seed retains meander point count"),
		Points.Num(), Again.Rivers[0].MeanderPoints.Num());
	bool bDifferentInterior = false;
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		TestTrue(TEXT("Same seed has identical final centerline"),
			Points[Index].Position == Again.Rivers[0].MeanderPoints[Index].Position);
		if (Different.Rivers[0].MeanderPoints.IsValidIndex(Index) &&
			Points[Index].Position != Different.Rivers[0].MeanderPoints[Index].Position)
		{
			bDifferentInterior = true;
		}
		FVoxelHydrologyInfluence Influence;
		TestTrue(TEXT("Final centerline is sampled by HydrologyPlan"),
			First.Sample(Points[Index].Position.X, Points[Index].Position.Y,
				Points[Index].WaterZ + 1, Influence) &&
			Influence.RiverZone != EVoxelRiverSurfaceZone::None);
		if (Index > 1)
		{
			const FIntPoint A = Points[Index - 1].Position - Points[Index - 2].Position;
			const FIntPoint B = Points[Index].Position - Points[Index - 1].Position;
			const int64 Dot = static_cast<int64>(A.X) * B.X +
				static_cast<int64>(A.Y) * B.Y;
			const int64 Cross = FMath::Abs(static_cast<int64>(A.X) * B.Y -
				static_cast<int64>(A.Y) * B.X);
			TestTrue(TEXT("Final river does not reverse upstream"), Dot > 0);
			TestTrue(TEXT("Final river turn stays within the configured angle"),
				Dot > 0 && Cross * 1024 <= Dot * 717);
		}
	}
	TestTrue(TEXT("Different seed changes interior bends"), bDifferentInterior);
	TestTrue(TEXT("Different seed preserves source anchor"),
		Different.Rivers[0].MeanderPoints[0].Position == Points[0].Position);
	TestTrue(TEXT("Different seed preserves confluence anchor"),
		Different.Rivers[0].MeanderPoints.Last().Position == Points.Last().Position);
	FString Error;
	TestTrue(TEXT("Final route satisfies river plan invariants"),
		First.ValidateRiverRoutes(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyContinuationAnchorTest,
	"WHFramework.Voxel.Hydrology.ContinuationAnchor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyContinuationAnchorTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const TArray<FIntPoint> Path = {
		FIntPoint(0, 0), FIntPoint(32, 0), FIntPoint(64, 0),
		FIntPoint(96, 8), FIntPoint(128, 8)};
	auto Build = [&Path](const int32 FirstIndex, const FIntPoint& CoreMin,
		const FIntPoint& CoreMax)
	{
		FVoxelHydrologyPlan Plan;
		Plan.Grid.WorldMinCell = FIntPoint(-8, -8);
		Plan.Grid.Width = 32;
		Plan.Grid.Height = 32;
		Plan.Grid.CellSize = 32;
		Plan.CoreMin = CoreMin;
		Plan.CoreMax = CoreMax;
		Plan.RiverSeed = 114;
		Plan.RiverShapeSmoothingPasses = 2;
		Plan.RiverMeanderStrength = 20;
		Plan.RiverMeanderFrequency = 96;
		Plan.RiverMeanderOctaves = 2;
		FVoxelRiverRoute& Route = Plan.Rivers.AddDefaulted_GetRef();
		Route.bContinuation = FirstIndex > 0;
		for (int32 Index = FirstIndex; Index < Path.Num(); ++Index)
		{
			FVoxelRiverRoutePoint& Point = Route.Points.AddDefaulted_GetRef();
			Point.Position = Path[Index];
			Point.WaterZ = 20 - Index;
			Point.BedHalfWidth = 4;
			Point.WaterHalfWidth = 4;
			Point.BankWidth = 4;
			Point.ShoreWidth = 8;
			Point.Depth = 4;
		}
		Plan.Finalize();
		return Plan;
	};
	const FVoxelHydrologyPlan Upstream = Build(0, FIntPoint(0, -64),
		FIntPoint(64, 64));
	const FVoxelHydrologyPlan Downstream = Build(1, FIntPoint(64, -64),
		FIntPoint(128, 64));
	const FIntPoint Handoff = FIntPoint(48, 0);
	bool bJoined = false;
	for (const FVoxelRiverMeanderPoint& Point : Upstream.Rivers[0].MeanderPoints)
	{
		bJoined |= Point.bAnchor && Point.Position == Handoff;
	}
	TestTrue(TEXT("Upstream meander preserves the region handoff"), bJoined);
	bool bDownstreamJoined = false;
	for (const FVoxelRiverMeanderPoint& Point : Downstream.Rivers[0].MeanderPoints)
	{
		bDownstreamJoined |= Point.bAnchor && Point.Position == Handoff;
	}
	TestTrue(TEXT("Continuation retains the same boundary anchor"),
		bDownstreamJoined);
	FVoxelHydrologyInfluence Left;
	FVoxelHydrologyInfluence Right;
	TestTrue(TEXT("Both regions sample wet river at the handoff"),
		Upstream.Sample(Handoff.X, Handoff.Y, 20, Left) && Left.bRiver &&
		Downstream.Sample(Handoff.X, Handoff.Y, 20, Right) && Right.bRiver);
	FString Error;
	TestTrue(TEXT("Upstream turns satisfy the final angle contract"),
		Upstream.ValidateRiverRoutes(Error));
	TestTrue(TEXT("Continuation turns satisfy the final angle contract"),
		Downstream.ValidateRiverRoutes(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelRiverSurfaceKeepsSubsoilTest,
	"WHFramework.Voxel.Hydrology.RiverSurfaceKeepsSubsoil",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelRiverSurfaceKeepsSubsoilTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationRecipe Recipe;
	Recipe.Palette.Grass = 3;
	Recipe.Palette.Dirt = 2;
	Recipe.Palette.Stone = 1;
	Recipe.Palette.Sand = 4;
	FVoxelSurfaceRuntimeRuleSet& Rules = Recipe.SurfaceRules.AddDefaulted_GetRef();
	FVoxelSurfaceRuntimeRule& Grass = Rules.Rules.AddDefaulted_GetRef();
	Grass.MinDepth = 0;
	Grass.MaxDepth = 0;
	Grass.BlockSymbol = Recipe.Palette.Grass;
	FVoxelSurfaceRuntimeRule& Stone = Rules.Rules.AddDefaulted_GetRef();
	Stone.MinDepth = 1;
	Stone.MaxDepth = 3;
	Stone.BlockSymbol = Recipe.Palette.Stone;
	Recipe.Biomes.AddDefaulted_GetRef().SurfaceRuleIndex = 0;
	const FVoxelSurfaceGenerator Surface(
		MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe)));
	FVoxelColumnSample Column;
	Column.BiomeIndex = 0;
	Column.bRiver = true;
	Column.RiverZone = EVoxelRiverSurfaceZone::ChannelBed;
	Column.RiverDistanceCells = 0;
	Column.BankDistanceCells = 0;
	Surface.ResolveColumn(16, 0, Column);
	TestEqual(TEXT("Riverbed top keeps the existing subsoil symbol"), Column.SurfaceMaterial, uint16(1));
	TestEqual(TEXT("Exact riverbed agrees with distant surface"),
		Surface.ResolveSymbol(FIntVector(16, 0, Column.SurfaceZ), Column, 0), uint32(1));
	Column.bRiver = false;
	Column.RiverZone = EVoxelRiverSurfaceZone::DryBank;
	Column.RiverDistanceCells = 6;
	int32 SandCount = 0;
	int32 GrassCount = 0;
	for (int32 X = 0; X < 100; ++X)
	{
		Surface.ResolveColumn(X, 0, Column);
		SandCount += Column.SurfaceMaterial == 4 ? 1 : 0;
		GrassCount += Column.SurfaceMaterial == 3 ? 1 : 0;
		TestEqual(TEXT("Fine and distant dry-bank material agree"),
			Surface.ResolveSymbol(FIntVector(X, 0, Column.SurfaceZ), Column, 0),
			static_cast<uint32>(Column.SurfaceMaterial));
	}
	TestTrue(TEXT("Dry bank contains some sand"), SandCount > 0);
	TestTrue(TEXT("Dry bank retains ordinary ground between sand patches"), GrassCount > 0);
	Column.BankDistanceCells = 2;
	Column.RiverZone = EVoxelRiverSurfaceZone::Floodplain;
	Surface.ResolveColumn(16, 0, Column);
	TestEqual(TEXT("Outside the riverbank, the ordinary grass rule still applies"),
		Column.SurfaceMaterial, uint16(3));
	Column.BiomeIndex = MAX_uint16;
	Column.BankDistanceCells = 0;
	Column.RiverZone = EVoxelRiverSurfaceZone::ChannelBed;
	Surface.ResolveColumn(16, 0, Column);
	TestEqual(TEXT("Riverbed without a biome rule keeps default dirt subsoil"),
		Column.SurfaceMaterial, uint16(2));
	Column = FVoxelColumnSample();
	Column.BiomeIndex = 0;
	Column.SurfaceZ = 8;
	Column.SurfaceWaterZ = 10;
	Column.bOcean = true;
	Surface.ResolveColumn(16, 0, Column);
	TestEqual(TEXT("Ocean floor keeps the existing subsoil symbol"),
		Column.SurfaceMaterial, uint16(1));
	Column.bOcean = false;
	Column.bLake = true;
	Surface.ResolveColumn(16, 0, Column);
	TestEqual(TEXT("Lake floor keeps the existing subsoil symbol"),
		Column.SurfaceMaterial, uint16(1));
	Column.bLake = false;
	Surface.ResolveColumn(16, 0, Column);
	TestEqual(TEXT("Any natural water-covered ground keeps subsoil"),
		Column.SurfaceMaterial, uint16(1));
	Column.SurfaceWaterZ = 7;
	Surface.ResolveColumn(16, 0, Column);
	TestEqual(TEXT("Ground above the water keeps its ordinary surface rule"),
		Column.SurfaceMaterial, uint16(3));
	return true;
}

#endif
