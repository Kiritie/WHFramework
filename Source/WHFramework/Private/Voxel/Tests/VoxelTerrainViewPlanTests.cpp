#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Rendering/VoxelTerrainViewPlan.h"
#include "Voxel/Rendering/VoxelTerrainStagePlanner.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"

namespace
{
	void SplitSpatialFixture(FVoxelTerrainViewPlan& InOutPlan, const FVoxelViewKey& InNode, const bool bFineDependency = false)
	{
		check(InOutPlan.Leaves.Contains(InNode));
		InOutPlan.Leaves.Remove(InNode);
		TArray<FVoxelViewKey> Children;
		InNode.GetChildren(Children);
		if (bFineDependency) InOutPlan.FineDependencies.Add(InNode);
		for (const FVoxelViewKey& Child : Children)
		{
			InOutPlan.Leaves.Add(Child);
			InOutPlan.Required.Add(Child);
			if (bFineDependency) InOutPlan.FineDependencies.Add(Child);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainPartitionTest, "WHFramework.Voxel.Rendering.Plan.Partition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainPartitionTest::RunTest(const FString& InParameters)
{
	FVoxelTerrainViewPlan Plan;
	const TSet<FIntVector> Fine = { FIntVector(-1, -1, -1), FIntVector(0, 0, 0) };
	Plan.Roots = {{FIntVector(-1), 1}, {FIntVector::ZeroValue, 1}};
	Plan.Leaves = Plan.Required = Plan.Roots;
	for (const FVoxelViewKey& Root : Plan.Roots) SplitSpatialFixture(Plan, Root);
	FString Error;
	TestTrue(TEXT("Fine coverage may exceed budget but cannot contain holes"), Plan.Validate(Error));
	TestEqual(TEXT("Negative and positive octants have separate roots"), Plan.Roots.Num(), 2);
	TestEqual(TEXT("Both roots have complete child families"), Plan.Leaves.Num(), 16);
	for (const FIntVector& Key : Fine)
	{
		TestTrue(TEXT("Requested fine section is a leaf"), Plan.Leaves.Contains({ Key, 0 }));
	}
	Plan.Leaves.Remove({ FIntVector(-1, -1, -1), 0 });
	TestFalse(TEXT("Missing child is detected"), Plan.Validate(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainHandoffTest, "WHFramework.Voxel.Rendering.Plan.BidirectionalHandoff", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainHandoffTest::RunTest(const FString& InParameters)
{
	const FVoxelViewKey Root { FIntVector(-1, 0, 0), 1 };
	FVoxelTerrainViewPlan Plan;
	Plan.Roots = {Root};
	Plan.Leaves = Plan.Required = Plan.Roots;
	SplitSpatialFixture(Plan, Root);
	TSet<FVoxelViewKey> Ready { Root };
	TSet<FVoxelViewKey> Visible;
	const TSet<FVoxelViewKey> Presented { Root };
	auto IsReady = [&Ready](const FVoxelViewKey& Key) { return Ready.Contains(Key); };
	Plan.ResolveVisible(IsReady, Visible, &Presented);
	TestTrue(TEXT("Parent is retained before children complete"), Visible.Num() == 1 && Visible.Contains(Root));
	TArray<FVoxelViewKey> Children;
	Root.GetChildren(Children);
	for (int32 Index = 0; Index < Children.Num() - 1; ++Index)
	{
		Ready.Add(Children[Index]);
		Plan.ResolveVisible(IsReady, Visible, &Presented);
		TestTrue(TEXT("Partial family cannot replace parent"), Visible.Num() == 1 && Visible.Contains(Root));
	}
	Ready.Add(Children.Last());
	Plan.ResolveVisible(IsReady, Visible, &Presented);
	TestEqual(TEXT("Complete family replaces parent"), Visible.Num(), 8);
	TestFalse(TEXT("Parent never overlaps children"), Visible.Contains(Root));
	const TSet<FVoxelViewKey> Previous = Visible;
	Ready.Reset();
	Plan.ResolveVisible(IsReady, Visible, &Previous);
	TestEqual(TEXT("Invalidation preserves committed empty and nonempty siblings"), Visible.Num(), 8);
	TestTrue(TEXT("Invalidation retains the exact previous cut"), Visible.Difference(Previous).IsEmpty());
	Plan.Leaves = Plan.Required = Plan.Roots;
	Ready.Remove(Root);
	Plan.ResolveVisible(IsReady, Visible, &Previous);
	TestEqual(TEXT("Merge retains previous children while parent builds"), Visible.Num(), 8);
	Ready.Add(Root);
	Plan.ResolveVisible(IsReady, Visible, &Previous);
	TestTrue(TEXT("Ready merged parent atomically replaces children"), Visible.Num() == 1 && Visible.Contains(Root));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainScreenErrorSymmetryTest, "WHFramework.Voxel.Rendering.Plan.ScreenErrorSymmetry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainScreenErrorSymmetryTest::RunTest(const FString& InParameters)
{
	FVoxelTerrainStageSource Left;
	Left.Source.Center = FIntVector(-48, 0, 0);
	Left.Source.VerticalFovDegrees = 90.0f;
	Left.Source.ViewportHeightPixels = 600;
	Left.OuterRadiusCells = 96;
	Left.VerticalHalfExtentCells = 32;
	FVoxelTerrainStageSource Right = Left;
	Right.Source.Center.X = 48;
	const TArray<FVoxelTerrainStageSource> Sources{Left, Right};
	const TArray<FVoxelTerrainStageSource> Reordered{Right, Left};
	FVoxelTerrainStagePlan First;
	FVoxelTerrainStagePlan Second;
	FString Error;
	if (!TestTrue(TEXT("Actual stage planner resolves both sources"),
		FVoxelTerrainStagePlanner::Build(EVoxelTerrainRepresentation::VoxelProxy, Sources, 3, 64, -32, 64, 8.0, 17, 23, First, Error)) ||
		!TestTrue(TEXT("Source order can change without changing sampling policy"),
		FVoxelTerrainStagePlanner::Build(EVoxelTerrainRepresentation::VoxelProxy, Reordered, 3, 64, -32, 64, 8.0, 17, 23, Second, Error))) return false;
	TestTrue(TEXT("Local SSE refines near geometry"), First.Pages.Num() > 2);
	TestEqual(TEXT("Source order preserves the complete geometry count"), First.Pages.Num(), Second.Pages.Num());
	for (const auto& Pair : First.Pages)
	{
		const FVoxelTerrainGeometry* Other = Second.Pages.Find(Pair.Key);
		TestTrue(TEXT("Source order preserves each actual sampling identity"), Other && *Other == Pair.Value);
		const FVoxelTerrainSpatialKey Reflected{Pair.Key.Representation,
			FIntVector(-Pair.Key.Coordinate.X - 1, Pair.Key.Coordinate.Y, Pair.Key.Coordinate.Z), Pair.Key.Level};
		const FVoxelTerrainGeometry* Mirror = First.Pages.Find(Reflected);
		TestTrue(TEXT("Mirrored sources preserve spatial and sampling symmetry"), Mirror && Mirror->SampleStepCells == Pair.Value.SampleStepCells);
		TestTrue(TEXT("Actual Proxy geometry obeys its representation floor"), Pair.Value.SampleStepCells >= 2 && Pair.Value.Validate(Error));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainNeighborBalanceTest, "WHFramework.Voxel.Rendering.Plan.NeighborBalance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainNeighborBalanceTest::RunTest(const FString& InParameters)
{
	FVoxelTerrainStageSource Source;
	Source.Source.Center = FIntVector(-8, 8, 8);
	Source.Source.VerticalFovDegrees = 90.0f;
	Source.Source.ViewportHeightPixels = 1080;
	Source.OuterRadiusCells = 256;
	Source.VerticalHalfExtentCells = 64;
	FVoxelTerrainStagePlan Plan;
	FString Error;
	if (!TestTrue(TEXT("Production coarse planner balances actual spatial pages"),
		FVoxelTerrainStagePlanner::Build(EVoxelTerrainRepresentation::VoxelProxy, MakeArrayView(&Source, 1),
			4, 64, -64, 128, 8.0, 17, 23, Plan, Error))) return false;
	TestFalse(TEXT("Balanced demand contains geometry"), Plan.Pages.IsEmpty());
	for (const auto& Pair : Plan.Pages)
	{
		TestTrue(TEXT("Balanced page retains its own valid sampling identity"), Pair.Value.Validate(Error));
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			for (const int32 Sign : {-1, 1})
			{
				FVoxelTerrainSpatialKey Neighbor = Pair.Key;
				Neighbor.Coordinate[Axis] += Sign;
				while (Neighbor.Level <= 4 && !Plan.Pages.Contains(Neighbor))
				{
					Neighbor.Coordinate = FIntVector(int32(VoxelStreamingGeometry::FloorDivide(Neighbor.Coordinate.X, 2)),
						int32(VoxelStreamingGeometry::FloorDivide(Neighbor.Coordinate.Y, 2)),
						int32(VoxelStreamingGeometry::FloorDivide(Neighbor.Coordinate.Z, 2)));
					++Neighbor.Level;
				}
				TestTrue(TEXT("Every present face neighbour differs by at most one spatial level"),
					Neighbor.Level > 4 || Neighbor.Level <= Pair.Key.Level + 1);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainFineDependenciesTest, "WHFramework.Voxel.Rendering.Plan.FineDependencies", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainFineDependenciesTest::RunTest(const FString& InParameters)
{
	FVoxelTerrainViewPlan Plan;
	const FVoxelViewKey Root { FIntVector(-1, 0, 0), 5 };
	const FVoxelViewKey Fine { FIntVector(-1, 0, 0), 0 };
	Plan.Roots = {Root};
	Plan.Leaves = Plan.Required = Plan.Roots;
	for (uint8 Level = Root.Level; Level > 0; --Level)
	{
		SplitSpatialFixture(Plan, {Fine.Coordinate, Level}, true);
	}
	TSet<FVoxelViewKey> Visible;
	Plan.ResolveVisible([&Plan](const FVoxelViewKey& Key) { return Plan.FineDependencies.Contains(Key); }, Visible);
	TestTrue(TEXT("Fine region can publish using only its dependency closure"), Visible.Contains(Fine));
	TestTrue(TEXT("Dependency closure remains smaller than the whole fine volume"), Plan.FineDependencies.Num() < 64);
	for (const FVoxelViewKey& Key : Plan.FineDependencies)
	{
		TestTrue(TEXT("Every dependency is a scheduled build"), Plan.Required.Contains(Key));
	}
	Plan.Leaves = Plan.Required = Plan.Roots;
	Plan.FineDependencies.Reset();
	TestTrue(TEXT("Obsolete fine dependencies are cleared"), Plan.FineDependencies.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainInitialFineTest, "WHFramework.Voxel.Rendering.Plan.InitialCoarseFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainInitialFineTest::RunTest(const FString& InParameters)
{
	FVoxelTerrainViewPlan Plan;
	const FVoxelViewKey Root { FIntVector(0, 0, 0), 4 };
	const FVoxelViewKey Fine { FIntVector(0, 0, 0), 0 };
	Plan.Roots = {Root};
	Plan.Leaves = Plan.Required = Plan.Roots;
	for (uint8 Level = Root.Level; Level > 0; --Level)
	{
		SplitSpatialFixture(Plan, {Fine.Coordinate, Level});
	}
	TSet<FVoxelViewKey> Ready { Root };
	TSet<FVoxelViewKey> Visible;
	auto IsReady = [&Ready](const FVoxelViewKey& Key) { return Ready.Contains(Key); };
	Plan.ResolveVisible(IsReady, Visible);
	TestTrue(TEXT("Ready coarse ancestor fills missing initial children"), Visible.Num() == 1 && Visible.Contains(Root));
	Ready.Add(Fine);
	Plan.ResolveVisible(IsReady, Visible);
	TestTrue(TEXT("Incomplete fine siblings retain the coarse ancestor"), Visible.Num() == 1 && Visible.Contains(Root));
	Ready.Reset();
	const TSet<FVoxelViewKey> Previous = Visible;
	Plan.ResolveVisible(IsReady, Visible, &Previous);
	TestTrue(TEXT("Invalidated coarse placeholder remains owned"), Visible.Num() == 1 && Visible.Contains(Root));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainSparseResolveTest, "WHFramework.Voxel.Rendering.Plan.SparseResolve", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainSparseResolveTest::RunTest(const FString& InParameters)
{
	FVoxelTerrainViewPlan Plan;
	for (int32 X = -4; X < 4; ++X)
	{
		for (int32 Y = -4; Y < 4; ++Y) Plan.Roots.Add({FIntVector(X, Y, 0), 1});
	}
	Plan.Leaves = Plan.Required = Plan.Roots;
	for (const FVoxelViewKey& Root : Plan.Roots) SplitSpatialFixture(Plan, Root);
	TSet<FVoxelViewKey> Ready;
	TSet<FVoxelViewKey> Previous;
	for (int32 Step = 0; Step < 12; ++Step)
	{
		Ready.Add({ FIntVector(Step - 6, 0, 0), 0 });
		if (Step == 6) Ready.Reset(); // 已提交空节点与失效节点仍须保留所有权。
		TSet<FVoxelViewKey> Reference;
		TSet<FVoxelViewKey> Pruned;
		TSet<FVoxelViewKey> Cached;
		TSet<FVoxelViewKey> ReadyBranches;
		for (FVoxelViewKey Node : Ready)
		{
			while (Node.Level < 24)
			{
				ReadyBranches.Add(Node);
				Node = Node.GetParent();
			}
		}
		int32 ReferenceQueries = 0;
		int32 PrunedQueries = 0;
		Plan.ResolveVisible([&](const FVoxelViewKey& Key) { ++ReferenceQueries; return Ready.Contains(Key); }, Reference, &Previous);
		Plan.ResolveVisible([&](const FVoxelViewKey& Key) { ++PrunedQueries; return Ready.Contains(Key); }, Pruned, &Previous, &Ready);
		Plan.ResolveVisible([&](const FVoxelViewKey& Key) { return Ready.Contains(Key); }, Cached, &Previous, nullptr, &ReadyBranches);
		TestTrue(TEXT("Pruning preserves the complete visible ownership set"), Reference.Num() == Pruned.Num() && Reference.Difference(Pruned).IsEmpty());
		TestTrue(TEXT("Cached branches preserve visible ownership after invalidation"), Reference.Num() == Cached.Num() && Reference.Difference(Cached).IsEmpty());
		TestTrue(TEXT("Unbuilt branches do not query thousands of missing leaves"), PrunedQueries < ReferenceQueries / 4);
		const TSet<FVoxelViewKey> ResolveRoots{*Plan.Roots.CreateConstIterator()};
		FVoxelTerrainViewPlan ScopedReference = Plan;
		ScopedReference.Roots = ResolveRoots;
		TSet<FVoxelViewKey> ExpectedScope, ActualScope;
		ScopedReference.ResolveVisible([&Ready](const FVoxelViewKey& Key) { return Ready.Contains(Key); },
			ExpectedScope, &Previous, nullptr, &ReadyBranches);
		Plan.ResolveVisible([&Ready](const FVoxelViewKey& Key) { return Ready.Contains(Key); },
			ActualScope, &Previous, nullptr, &ReadyBranches, &ResolveRoots);
		TestTrue(TEXT("Resolving selected roots reuses the complete plan with unchanged ownership semantics"),
			ExpectedScope.Num() == ActualScope.Num() && ExpectedScope.Difference(ActualScope).IsEmpty());
		Previous = MoveTemp(Pruned);
	}
	return true;
}

#endif
