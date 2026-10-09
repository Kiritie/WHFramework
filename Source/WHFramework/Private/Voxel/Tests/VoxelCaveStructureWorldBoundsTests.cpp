#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Voxel/Generation/Caves/VoxelCaveGenerator.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelStructure.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

namespace
{
	class FVoxelWorldBoundsStructureLayout final : public IVoxelStructureLayoutAlgorithm
	{
	public:
		virtual FName GetId() const override
		{
			return TEXT("test:world_bounds_structure_layout");
		}

		virtual uint32 GetVersion() const override
		{
			return 1;
		}

		virtual bool BuildLayout(const FVoxelStructureLayoutContext& InContext,
			TArray<FVoxelStructurePiecePlacement>& OutPieces, FString& OutError) const override
		{
			OutPieces.Reset();
			if (!InContext.ReservePieces(OutPieces, 1, OutError)) return false;
			OutPieces.Add({0, FInt64Vector(0, MIN_int32, 0), 0});
			OutError.Reset();
			return true;
		}
	};

	FVoxelStructureRuntimeDefinition MakeWorldBoundsStructure()
	{
		FVoxelStructureRuntimeDefinition Definition;
		Definition.StableId = TEXT("test:world_bounds_structure");
		Definition.StableHash = 0x721438FE419ABB10ull;
		Definition.Stage = EVoxelGenerationStage::SurfaceStructures;
		Definition.Placement.Spacing = 1;
		Definition.Placement.Separation = 0;
		Definition.Placement.ChancePermille = 1000;
		Definition.Placement.bRequireSurface = true;
		FVoxelStructurePieceTemplate Piece;
		Piece.StableId = TEXT("test:world_bounds_piece");
		Piece.Bounds = {FIntVector::ZeroValue, FIntVector(3, 1, 1)};
		Piece.Writes.Add({FIntVector::ZeroValue, 3, 1});
		Piece.ClearVolumes.Add(Piece.Bounds);
		Piece.Details.Add({TEXT("test:inside"), FIntVector::ZeroValue, 0});
		Piece.Details.Add({TEXT("test:outside"), FIntVector(1, 0, 0), 0});
		Definition.Pieces.Add(MoveTemp(Piece));
		return Definition;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCaveWorldBoundsTest,
	"WHFramework.Voxel.Cave.WorldBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCaveWorldBoundsTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationRecipe Recipe = *VoxelTest::MakeGenerationConfig()->Recipe;
	Recipe.Settings.CaveSpacing = 64;
	Recipe.Settings.CaveMainRadius = 2;
	Recipe.Settings.CaveBranchRadius = 2;
	Recipe.Settings.CaveMinDepth = 4;
	Recipe.Settings.CaveSystemChancePermille = 1000;
	Recipe.Settings.CaveEntranceChancePermille = 1000;
	Recipe.Settings.CaveEntranceLength = 32;
	Recipe.Settings.CaveEntranceClearance = 4;
	Recipe.Settings.CaveRoomChancePermille = 0;
	Recipe.Settings.CaveBranchChancePermille = 1000;
	const FVoxelCaveGenerator Generator(MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe)));
	for (const int32 Origin : {MIN_int32, MAX_int32 - 128})
	{
		const FVoxelGenerationBounds Bounds{FIntVector(Origin, Origin, -64), FIntVector(Origin + 128, Origin + 128, 128)};
		bool bSamplesStayNearOwner = true;
		int32 SampleCount = 0;
		const auto SampleColumn = [&Bounds, &bSamplesStayNearOwner, &SampleCount](
			const FIntVector& Position, FVoxelColumnSample& Column)
		{
			++SampleCount;
			bSamplesStayNearOwner &= VoxelGeneration::IsWorldCell(Position.X) && VoxelGeneration::IsWorldCell(Position.Y) &&
				FMath::Abs(int64(Position.X) - Bounds.Min.X) < 512 && FMath::Abs(int64(Position.Y) - Bounds.Min.Y) < 512;
			Column.SurfaceZ = 96;
			return true;
		};
		FVoxelCavePlan Plan;
		FVoxelCavePlan Repeat;
		FString Error;
		if (!TestTrue(TEXT("Native cave generator consumes boundary owner"), Generator.BuildPlan(Bounds, SampleColumn, Plan, Error)) ||
			!TestTrue(TEXT("Boundary cave repeat builds"), Generator.BuildPlan(Bounds, SampleColumn, Repeat, Error)))
		{
			return false;
		}
		TestTrue(TEXT("Native boundary cave performs sampling"), SampleCount > 0);
		TestTrue(TEXT("Boundary sampling never wraps to the other world edge"), bSamplesStayNearOwner);
		TestTrue(TEXT("Boundary owner has actual cave segments"), !Plan.Segments.IsEmpty());
		TestEqual(TEXT("Boundary repeat retains segment count"), Plan.Segments.Num(), Repeat.Segments.Num());
		for (int32 Index = 0; Index < Plan.Segments.Num(); ++Index)
		{
			const FVoxelCaveSegment& Segment = Plan.Segments[Index];
			TestTrue(TEXT("Native cave endpoints stay in world domain"), VoxelGeneration::IsWorldCell(Segment.Start.X) &&
				VoxelGeneration::IsWorldCell(Segment.Start.Y) && VoxelGeneration::IsWorldCell(Segment.Start.Z) &&
				VoxelGeneration::IsWorldCell(Segment.End.X) && VoxelGeneration::IsWorldCell(Segment.End.Y) &&
				VoxelGeneration::IsWorldCell(Segment.End.Z));
			TestTrue(TEXT("Finalized boundary segment remains consumable"), Plan.Carves(Segment.Start));
			if (Repeat.Segments.IsValidIndex(Index))
			{
				TestEqual(TEXT("Boundary repeat retains start"), Segment.Start, Repeat.Segments[Index].Start);
				TestEqual(TEXT("Boundary repeat retains end"), Segment.End, Repeat.Segments[Index].End);
			}
		}
		for (const FVoxelGenerationBounds& Clear : Plan.ClearVolumes)
		{
			TestTrue(TEXT("Entrance commits valid complete clearance"), Clear.IsValid());
		}
		TestEqual(TEXT("Boundary repeat retains entrance count"), Plan.Entrances.Num(), Repeat.Entrances.Num());
		for (int32 Index = 0; Index < Plan.Entrances.Num() && Index < Repeat.Entrances.Num(); ++Index)
		{
			TestTrue(TEXT("Boundary entrances retain stable identity"), Plan.Entrances[Index].Id == Repeat.Entrances[Index].Id);
		}
	}
	for (const int32 Edge : {MIN_int32, MAX_int32 - 1})
	{
		FVoxelCavePlan Plan;
		Plan.Segments.Add({FIntVector(Edge, 0, 0), FIntVector(Edge, 0, 0), 4});
		Plan.Finalize();
		TestTrue(TEXT("Native Finalize clips capsule extent and indexes edge cell"), Plan.Carves(FIntVector(Edge, 0, 0)));
		TestFalse(TEXT("Exclusive upper world cell cannot be carved"), Plan.Carves(FIntVector(MAX_int32, 0, 0)));
	}
	FVoxelGenerationRecipe WideRecipe = *VoxelTest::MakeGenerationConfig()->Recipe;
	WideRecipe.Settings.MinZ = MIN_int32;
	WideRecipe.Settings.MaxZ = MAX_int32;
	WideRecipe.Settings.CaveSpacing = 64;
	WideRecipe.Settings.CaveMainRadius = 2;
	WideRecipe.Settings.CaveBranchRadius = 2;
	WideRecipe.Settings.CaveMinDepth = 4;
	WideRecipe.Settings.CaveSystemChancePermille = 1000;
	WideRecipe.Settings.CaveEntranceChancePermille = 1000;
	WideRecipe.Settings.CaveRoomChancePermille = 0;
	WideRecipe.Settings.CaveBranchChancePermille = 0;
	const FVoxelCaveGenerator WideGenerator(MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(WideRecipe)));
	FVoxelCavePlan WidePlan;
	FString WideError;
	TestTrue(TEXT("Native cave start supports a vertical random range wider than int32"), WideGenerator.BuildPlan(
		{FIntVector(0, 0, MIN_int32), FIntVector(128, 128, MAX_int32)},
		[](const FIntVector&, FVoxelColumnSample& Column)
		{
			Column.SurfaceZ = MAX_int32 - 3;
			return true;
		}, WidePlan, WideError));
	TestEqual(TEXT("Required entrance clearance beyond world rejects the entrance"), WidePlan.Entrances.Num(), 0);
	TestEqual(TEXT("Rejected entrance does not commit partial clear volumes"), WidePlan.ClearVolumes.Num(), 0);
	bool bHasInteriorStart = false;
	for (const FVoxelCaveSegment& Segment : WidePlan.Segments)
	{
		TestTrue(TEXT("Wide vertical cave has valid endpoint cells"), VoxelGeneration::IsWorldCell(Segment.Start.Z) &&
			VoxelGeneration::IsWorldCell(Segment.End.Z));
		bHasInteriorStart |= int64(Segment.Start.Z) > int64(MIN_int32) + 1024 &&
			int64(Segment.Start.Z) < int64(MAX_int32) - 1024;
	}
	TestTrue(TEXT("Wide random range does not collapse to its lower endpoint"), bHasInteriorStart);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelStructureWorldBoundsTest,
	"WHFramework.Voxel.Generation.StructureWorldBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStructureWorldBoundsTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationRecipe Recipe = *VoxelTest::MakeGenerationConfig()->Recipe;
	Recipe.Structures.Empty();
	const FVoxelStructureRuntimeDefinition Definition = MakeWorldBoundsStructure();
	Recipe.Structures.Add(Definition);
	Recipe.Biomes[0].StructureIndices = {0};
	FString Error;
	if (!TestTrue(TEXT("Boundary structure recipe lookups build"), Recipe.BuildLookups(Error)))
	{
		return false;
	}
	const FVoxelStructurePlanner Planner(MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe)));
	FVoxelPlannedStructurePlacement Placement;
	Placement.Id = VoxelGeneration::MakeStableId(71, FIntVector(MAX_int32 - 1, 0, 1), 9);
	Placement.DefinitionId = Definition.StableId;
	Placement.Anchor = FIntVector(MAX_int32 - 1, 0, 1);
	FVoxelStructureInstance Instance;
	if (!TestTrue(TEXT("Native exact structure builds at last world cell"), Planner.BuildAt(Placement, Instance, Error)))
	{
		return false;
	}
	TestTrue(TEXT("Clipped structure retains requested identity"), Instance.Id == Placement.Id);
	TestEqual(TEXT("Clipped structure retains exact anchor"), Instance.Anchor, Placement.Anchor);
	TestEqual(TEXT("Only in-domain native write is emitted"), Instance.Writes.Num(), 1);
	TestEqual(TEXT("Outside detail is skipped"), Instance.Details.Num(), 1);
	TestEqual(TEXT("Cross-boundary clear volume survives clipping"), Instance.ClearVolumes.Num(), 1);
	if (!Instance.ClearVolumes.IsEmpty())
	{
		TestEqual(TEXT("Clear upper endpoint uses exclusive world boundary"), Instance.ClearVolumes[0].Bounds.Max.X, MAX_int32);
	}
	Placement.Anchor.X = MAX_int32;
	TestFalse(TEXT("External excluded anchor is rejected without clamping"), Planner.BuildAt(Placement, Instance, Error));
	TestEqual(TEXT("Anchor rejection preserves accepted output"), Instance.Anchor.X, MAX_int32 - 1);
	for (const int32 Origin : {MIN_int32, MAX_int32 - 2})
	{
		const FVoxelGenerationBounds Bounds{FIntVector(Origin, 0, -4), FIntVector(Origin + 2, 2, 4)};
		bool bSamplesStayInOwner = true;
		int32 SampleCount = 0;
		TArray<FVoxelStructureInstance> Planned;
		TestTrue(TEXT("Native spacing-one candidate grid handles world edge"), Planner.Plan(Bounds,
			[&Bounds, &bSamplesStayInOwner, &SampleCount](const FIntVector& Position, FVoxelColumnSample& Column)
			{
				++SampleCount;
				bSamplesStayInOwner &= Bounds.Contains(Position);
				Column.SurfaceZ = 0;
				Column.BiomeIndex = 0;
				return true;
			}, Planned, Error));
		TestEqual(TEXT("Spacing-one boundary grid samples each real cell once"), SampleCount, 4);
		TestTrue(TEXT("Candidate grid neither wraps nor clamps anchors"), bSamplesStayInOwner);
	}
	for (const int32 SurfaceZ : {MAX_int32, MIN_int32})
	{
		FVoxelGenerationRecipe VerticalRecipe = *VoxelTest::MakeGenerationConfig()->Recipe;

		FVoxelStructureRuntimeDefinition VerticalDefinition = MakeWorldBoundsStructure();
		VerticalDefinition.Placement.MinZ = MIN_int32;
		VerticalDefinition.Placement.MaxZ = MAX_int32 - 1;
		VerticalDefinition.Adaptation = SurfaceZ == MIN_int32 ? EVoxelStructureAdaptation::Bury : EVoxelStructureAdaptation::None;
		VerticalRecipe.Structures = {VerticalDefinition};
		VerticalRecipe.Biomes[0].StructureIndices = {0};
		if (!TestTrue(TEXT("Vertical edge structure recipe builds"), VerticalRecipe.BuildLookups(Error)))
		{
			return false;
		}
		const FVoxelStructurePlanner VerticalPlanner(MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(VerticalRecipe)));
		TArray<FVoxelStructureInstance> Planned;
		int32 SampleCount = 0;
		TestTrue(TEXT("Native planner rejects invalid surface or buried anchor without wrapping"), VerticalPlanner.Plan(
			{FIntVector(0, 0, MIN_int32), FIntVector(1, 1, MAX_int32)},
			[SurfaceZ, &SampleCount](const FIntVector&, FVoxelColumnSample& Column)
			{
				++SampleCount;
				Column.SurfaceZ = SurfaceZ;
				Column.BiomeIndex = 0;
				return true;
			}, Planned, Error));
		TestEqual(TEXT("Vertical rejection follows actual column sampling"), SampleCount, 1);
		TestEqual(TEXT("Out-of-domain vertical anchor never becomes a structure"), Planned.Num(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelStructureWideOriginTest,
	"WHFramework.Voxel.Generation.StructureWideOrigin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStructureWideOriginTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Algorithm = MakeShared<const FVoxelWorldBoundsStructureLayout, ESPMode::ThreadSafe>();
	FString Error;
	if (!TestTrue(TEXT("Wide local structure layout registers"), FVoxelStructureLayoutRegistry::Get().Register(Algorithm, Error)))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		FVoxelStructureLayoutRegistry::Get().Unregister(Algorithm->GetId());
	};
	FVoxelGenerationRecipe Recipe = *VoxelTest::MakeGenerationConfig()->Recipe;
	Recipe.Structures.Empty();
	FVoxelStructureRuntimeDefinition Definition = MakeWorldBoundsStructure();
	Definition.LayoutAlgorithmId = Algorithm->GetId();
	Definition.LayoutAlgorithmVersion = Algorithm->GetVersion();
	Recipe.Structures.Add(Definition);
	if (!TestTrue(TEXT("Wide local recipe lookups build"), Recipe.BuildLookups(Error)))
	{
		return false;
	}
	const FVoxelStructurePlanner Planner(MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe)));
	FVoxelPlannedStructurePlacement Placement;
	Placement.Id = VoxelGeneration::MakeStableId(71, FIntVector(MIN_int32, 0, 0), 9);
	Placement.DefinitionId = Definition.StableId;
	Placement.Anchor = FIntVector(MIN_int32, 0, 0);
	Placement.Yaw = 1;
	FVoxelStructureInstance Instance;
	if (!TestTrue(TEXT("Native rotation retains wide origin until final cell"), Planner.BuildAt(Placement, Instance, Error)))
	{
		return false;
	}
	if (!TestEqual(TEXT("Wide layout retains placement metadata"), Instance.Pieces.Num(), 1) ||
		!TestEqual(TEXT("Wide layout has actual native writes"), Instance.Writes.Num(), 3) ||
		!TestEqual(TEXT("Wide layout has actual native details"), Instance.Details.Num(), 2))
	{
		return false;
	}
	TestEqual(TEXT("Rotated metadata records exact local origin"), Instance.Pieces[0].Origin.X, int64(MAX_int32) + 1);
	TestEqual(TEXT("Final native world write is not clamped"), Instance.Writes[0].Position, FIntVector(-1, 0, 0));
	TestEqual(TEXT("Final native detail uses corner transform"), Instance.Details[0].Position, FIntVector::ZeroValue);
	TestTrue(TEXT("Wide transform retains logical identity"), Instance.Id == Placement.Id);
	return true;
}

#endif
