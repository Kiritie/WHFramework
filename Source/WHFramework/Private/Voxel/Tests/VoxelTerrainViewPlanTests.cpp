#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Rendering/VoxelTerrainViewPlan.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainPartitionTest, "WHFramework.Voxel.Rendering.Plan.Partition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainPartitionTest::RunTest(const FString& InParameters)
{
	FVoxelTerrainViewPlan Plan;
	const TSet<FIntVector> Fine = { FIntVector(-1, -1, -1), FIntVector(0, 0, 0) };
	Plan.Build({}, Fine, {}, FVoxelViewSettings(), 8);
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
	Plan.Build({}, { FIntVector(-1, 0, 0) }, { Root }, FVoxelViewSettings(), 64);
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
	Plan.Build({}, {}, { Root }, FVoxelViewSettings(), 64);
	Ready.Remove(Root);
	Plan.ResolveVisible(IsReady, Visible, &Previous);
	TestEqual(TEXT("Merge retains previous children while parent builds"), Visible.Num(), 8);
	Ready.Add(Root);
	Plan.ResolveVisible(IsReady, Visible, &Previous);
	TestTrue(TEXT("Ready merged parent atomically replaces children"), Visible.Num() == 1 && Visible.Contains(Root));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainBudgetSymmetryTest, "WHFramework.Voxel.Rendering.Plan.BudgetSymmetry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainBudgetSymmetryTest::RunTest(const FString& InParameters)
{
	FVoxelStreamingSource Source;
	Source.RenderMode = EVoxelStreamingRenderMode::Full;
	Source.Center = FIntVector::ZeroValue;
	const FVoxelViewKey Left { FIntVector(-1, 0, 0), 3 };
	const FVoxelViewKey Right { FIntVector(0, 0, 0), 3 };
	FVoxelTerrainViewPlan Plan;
	Plan.Build(MakeArrayView(&Source, 1), {}, { Left, Right }, FVoxelViewSettings(), 9);
	TestEqual(TEXT("Insufficient budget cannot refine only one equally near side"), Plan.Leaves.Num(), 2);
	Plan.Build(MakeArrayView(&Source, 1), {}, { Right, Left }, FVoxelViewSettings(), 16);
	TestEqual(TEXT("Equal priority regions refine together"), Plan.Leaves.Num(), 16);
	FString Error;
	TestTrue(TEXT("Budgeted refinement preserves full partition"), Plan.Validate(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainNeighborBalanceTest, "WHFramework.Voxel.Rendering.Plan.NeighborBalance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainNeighborBalanceTest::RunTest(const FString& InParameters)
{
	FVoxelTerrainViewPlan Plan;
	Plan.Build({}, { FIntVector(-1, 0, 0) },
		{ { FIntVector(-1, 0, 0), 4 }, { FIntVector(0, 0, 0), 4 } }, FVoxelViewSettings(), 2);
	FString Error;
	TestTrue(TEXT("Balanced partition still covers both roots"), Plan.Validate(Error));
	TestTrue(TEXT("Mandatory fine and boundary budget excess is explicit"), Plan.OverBudgetLeaves > 0);
	for (const FVoxelViewKey& Node : Plan.Leaves)
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			for (int32 Sign = -1; Sign <= 1; Sign += 2)
			{
				FVoxelViewKey Neighbor = Node;
				Neighbor.Coordinate[Axis] += Sign;
				while (Neighbor.Level <= 4 && !Plan.Leaves.Contains(Neighbor))
				{
					Neighbor = Neighbor.GetParent();
				}
				TestTrue(TEXT("Each present face neighbour differs by at most one level"), Neighbor.Level > 4 || Neighbor.Level <= Node.Level + 1);
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
	Plan.Build({}, { Fine.Coordinate }, { Root }, FVoxelViewSettings(), 8192);
	TSet<FVoxelViewKey> Visible;
	Plan.ResolveVisible([&Plan](const FVoxelViewKey& Key) { return Plan.FineDependencies.Contains(Key); }, Visible);
	TestTrue(TEXT("Fine region can publish using only its dependency closure"), Visible.Contains(Fine));
	TestTrue(TEXT("Dependency closure remains smaller than the whole fine volume"), Plan.FineDependencies.Num() < 64);
	for (const FVoxelViewKey& Key : Plan.FineDependencies)
	{
		TestTrue(TEXT("Every dependency is a scheduled build"), Plan.Required.Contains(Key));
	}
	Plan.Build({}, {}, { Root }, FVoxelViewSettings(), 8192);
	TestTrue(TEXT("Obsolete fine dependencies are cleared"), Plan.FineDependencies.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainInitialFineTest, "WHFramework.Voxel.Rendering.Plan.InitialCoarseFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainInitialFineTest::RunTest(const FString& InParameters)
{
	FVoxelTerrainViewPlan Plan;
	const FVoxelViewKey Root { FIntVector(0, 0, 0), 4 };
	const FVoxelViewKey Fine { FIntVector(0, 0, 0), 0 };
	Plan.Build({}, { Fine.Coordinate }, { Root }, FVoxelViewSettings(), 8192);
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
	TSet<FIntVector> Fine;
	for (int32 X = -8; X < 8; ++X)
	{
		for (int32 Y = -8; Y < 8; ++Y) Fine.Add(FIntVector(X, Y, 0));
	}
	Plan.Build({}, Fine, {}, FVoxelViewSettings(), 8192);
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
		Previous = MoveTemp(Pruned);
	}
	return true;
}

#endif
