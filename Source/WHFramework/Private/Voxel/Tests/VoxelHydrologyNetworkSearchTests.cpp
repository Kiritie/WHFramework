#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyNetworkSearch.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyLocalRunoff.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Tests/VoxelTestUtilities.h"
#include "Voxel/Generation/VoxelGenerationMath.h"

namespace VoxelHydrologyNetworkSearchTests
{
	constexpr uint64 Generation = 401;
	constexpr int32 Side = 12;
	constexpr int32 Width = Side * 2;
	const FIntPoint Minimum(-Side, -Side);

	struct FRegion
	{
		FIntPoint Coordinate;
		FVoxelHydrologyRegionGraph Graph;
	};

	bool MakeRegions(TArray<FRegion>& OutRegions, const int32 InVariant, FString& OutError)
	{
		for (int32 Y = -1; Y <= 0; ++Y)
		{
			for (int32 X = -1; X <= 0; ++X)
			{
				FRegion& Region = OutRegions.AddDefaulted_GetRef();
				Region.Coordinate = FIntPoint(X, Y);
				FVoxelHydrologyRegionGraphBuild Build;
				if (!Build.Initialize(FIntPoint(X * Side, Y * Side), Side, Side, 32, Generation, OutError)) return false;
				const auto Read = [InVariant](const FIntPoint& Position, FVoxelHydrologyRegionSample& OutCell, FString& Error)
				{
					const FIntPoint Local = Position - Minimum;
					OutCell.GroundZ = InVariant < 6 ? (Local.X * 13 + Local.Y * 7 + InVariant * 11) % 17 - 5 : 8;
					OutCell.RainWeight = 2;
					if (Local.X == 0 || Local.Y == 0 || Local.X + 1 == Width || Local.Y + 1 == Width)
					{
						OutCell.GroundZ = -9;
						OutCell.WaterZ = 0;
						OutCell.WaterId = {11, 13};
					}
					Error.Reset();
					return EVoxelHydrologyClosureRead::Ready;
				};
				EVoxelHydrologyRegionStep Step = EVoxelHydrologyRegionStep::None;
				for (int32 Attempt = 0; Attempt < 10000; ++Attempt)
				{
					Step = Build.Advance(Generation, Read, 113, OutError);
					if (Step != EVoxelHydrologyRegionStep::Yielded) break;
				}
				if (Step != EVoxelHydrologyRegionStep::Ready) return false;
				Region.Graph = Build.TakeResult();
			}
		}
		return true;
	}

	const FVoxelHydrologyRegionGraph* FindRegion(const TArray<FRegion>& InRegions, const FIntPoint& InCoordinate)
	{
		for (const auto& Region : InRegions)
		{
			if (Region.Coordinate == InCoordinate) return &Region.Graph;
		}
		return nullptr;
	}

	int32 Index(const FIntPoint& InCell)
	{
		const FIntPoint Local = InCell - Minimum;
		return Local.X >= 0 && Local.Y >= 0 && Local.X < Width && Local.Y < Width ? Local.X + Local.Y * Width : INDEX_NONE;
	}

	bool CellLess(const FIntPoint& InFirst, const FIntPoint& InSecond)
	{
		return InFirst.X < InSecond.X || (InFirst.X == InSecond.X && InFirst.Y < InSecond.Y);
	}

	struct FExpanded
	{
		TArray<TArray<int32>> Neighbors;
		TArray<int32> Ground;
		TArray<uint8> Active;
		TArray<uint8> Wet;
		TArray<FVoxelHydrologyRegionPotential> Potentials;
		TArray<int32> Parents;

		FIntPoint Position(const int32 InIndex) const
		{
			return Minimum + FIntPoint(InIndex % Width, InIndex / Width);
		}

		void Link(const int32 First, const int32 Second)
		{
			Neighbors[First].AddUnique(Second);
			Neighbors[Second].AddUnique(First);
		}

		bool Solve(const TArray<FRegion>& InRegions, const bool bFullForest = false, const int32 InVariant = 0)
		{
			constexpr int32 Count = Width * Width;
			Neighbors.SetNum(Count);
			Ground.Init(0, Count);
			Active.Init(0, Count);
			Wet.Init(0, Count);
			Potentials.SetNum(Count);
			Parents.Init(INDEX_NONE, Count);
			for (const auto& Region : InRegions)
			{
				for (const auto& Node : Region.Graph.Nodes)
				{
					const int32 Cell = Index(Node.Cell);
					Ground[Cell] = Node.GroundZ;
					Active[Cell] = 1;
					if (Node.IsReceiver())
					{
						Wet[Cell] = 1;
						Potentials[Cell] = {Node.WaterZ, 0};
					}
				}
				for (int32 Profile = 0; Profile < Region.Graph.Profiles.Num(); ++Profile)
				{
					const int32 Cell = Index(Region.Graph.GetProfileCell(Profile));
					Ground[Cell] = Region.Graph.Profiles[Profile].GroundZ;
					Active[Cell] = 1;
				}
				for (const auto& Edge : Region.Graph.Edges)
				{
					int32 Previous = Index(Region.Graph.Nodes[Edge.First].Cell);
					for (int32 Offset = 0; Offset <= Edge.ProfileCount; ++Offset)
					{
						const int32 Current = Index(Offset < Edge.ProfileCount ? Region.Graph.GetProfileCell(Edge.ProfileOffset + Offset) : Region.Graph.Nodes[Edge.Second].Cell);
						Link(Previous, Current);
						Previous = Current;
					}
				}
			}
            if (bFullForest)
            {
                static const FIntPoint Offsets[] = {FIntPoint(-1, 0), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(0, 1)};
                for (const auto& Region : InRegions)
                {
                    for (int32 CellIndex = 0; CellIndex < Region.Graph.ForestNeighbors.Num(); ++CellIndex)
                    {
                        const FIntPoint Cell = Region.Graph.Minimum + FIntPoint(CellIndex % Side, CellIndex / Side);
                        const int32 Dense = Index(Cell);
                        const FIntPoint Local = Cell - Minimum;
                        Active[Dense] = 1;
                        Ground[Dense] = InVariant < 6 ? (Local.X * 13 + Local.Y * 7 + InVariant * 11) % 17 - 5 : 8;
                        if (Local.X == 0 || Local.Y == 0 || Local.X + 1 == Width || Local.Y + 1 == Width)
                        {
                            Ground[Dense] = -9;
                            Wet[Dense] = 1;
                            Potentials[Dense] = {0, 0};
                        }
                        for (int32 Face = 0; Face < 4; ++Face)
                            if (Region.Graph.ForestNeighbors[CellIndex] & (1 << Face)) Link(Dense, Index(Cell + Offsets[Face]));
                    }
                }
            }
			for (int32 Coordinate = 0; Coordinate < Width; ++Coordinate)
			{
				Link(Index(FIntPoint(-1, Minimum.Y + Coordinate)), Index(FIntPoint(0, Minimum.Y + Coordinate)));
				Link(Index(FIntPoint(Minimum.X + Coordinate, -1)), Index(FIntPoint(Minimum.X + Coordinate, 0)));
			}
			TArray<uint8> Visited;
			Visited.Init(0, Count);
			for (int32 Work = 0; Work < Count; ++Work)
			{
				int32 Current = INDEX_NONE;
				for (int32 Cell = 0; Cell < Count; ++Cell)
				{
					if (Active[Cell] && !Visited[Cell] && Potentials[Cell].SpillZ != MAX_int32 &&
						(Current == INDEX_NONE || Potentials[Cell].SpillZ < Potentials[Current].SpillZ)) Current = Cell;
				}
				if (Current == INDEX_NONE) break;
				Visited[Current] = 1;
				for (const int32 Neighbor : Neighbors[Current])
				{
					if (Wet[Neighbor]) continue;
					Potentials[Neighbor].SpillZ = FMath::Min(Potentials[Neighbor].SpillZ, FMath::Max(Potentials[Current].SpillZ, Ground[Neighbor]));
				}
			}
			TArray<int32> Queue;
			for (int32 Cell = 0; Cell < Count; ++Cell)
			{
				if (!Active[Cell]) continue;
				if (Potentials[Cell].SpillZ == MAX_int32) return false;
				if (Wet[Cell])
				{
					Queue.Add(Cell);
					continue;
				}
				for (const int32 Neighbor : Neighbors[Cell])
				{
					if (Potentials[Neighbor].SpillZ >= Potentials[Cell].SpillZ) continue;
					if (Parents[Cell] == INDEX_NONE || CellLess(Position(Neighbor), Position(Parents[Cell]))) Parents[Cell] = Neighbor;
				}
				if (Parents[Cell] != INDEX_NONE)
				{
					Potentials[Cell].PlateauDistance = 0;
					Queue.Add(Cell);
				}
			}
			for (int32 Cursor = 0; Cursor < Queue.Num(); ++Cursor)
			{
				const int32 Cell = Queue[Cursor];
				for (const int32 Neighbor : Neighbors[Cell])
				{
					if (Potentials[Neighbor].SpillZ != Potentials[Cell].SpillZ || Potentials[Neighbor].PlateauDistance != MAX_int64) continue;
					Potentials[Neighbor].PlateauDistance = Potentials[Cell].PlateauDistance + 1;
					Queue.Add(Neighbor);
				}
			}
			for (int32 Cell = 0; Cell < Count; ++Cell)
			{
				if (!Active[Cell] || Wet[Cell] || Parents[Cell] != INDEX_NONE) continue;
				for (const int32 Neighbor : Neighbors[Cell])
				{
					if (Potentials[Neighbor].SpillZ != Potentials[Cell].SpillZ || Potentials[Neighbor].PlateauDistance + 1 != Potentials[Cell].PlateauDistance) continue;
					if (Parents[Cell] == INDEX_NONE || CellLess(Position(Neighbor), Position(Parents[Cell]))) Parents[Cell] = Neighbor;
				}
				if (Parents[Cell] == INDEX_NONE) return false;
			}
			return true;
		}
	};

	bool Finish(FVoxelHydrologyNetworkSearch& InSearch, FVoxelHydrologyNetworkSearch::FReadRegion InRead,
		const int32 InBudget, FString& OutError)
	{
		for (int32 Attempt = 0; Attempt < 200000; ++Attempt)
		{
			const auto Step = InSearch.Advance(Generation, InRead, InBudget, OutError);
			if (Step == EVoxelHydrologyNetworkStep::Ready) return true;
			if (Step != EVoxelHydrologyNetworkStep::Yielded) return false;
		}
		OutError = TEXT("Network fixture exceeded its finite work bound");
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyNetworkPathsTest,
	"WHFramework.Voxel.Hydrology.Network.CrossRegionWeightedPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyNetworkPathsTest::RunTest(const FString& Parameters)
{
	using namespace VoxelHydrologyNetworkSearchTests;
	(void)Parameters;
	int32 CheckedParents = 0;
	int32 CheckedProfiles = 0;
	for (int32 Variant = 0; Variant < 8; ++Variant)
	{
		TArray<FRegion> Regions;
		FString Error;
		if (!MakeRegions(Regions, Variant, Error))
		{
			AddError(Error);
			return false;
		}
		FExpanded Expanded;
		if (!TestTrue(TEXT("独立展开跨区路径图完成逐格求解"), Expanded.Solve(Regions))) return false;
		const auto Read = [&](const FIntPoint& Coordinate, const FVoxelHydrologyRegionGraph*& OutGraph, FString& OutError)
		{
			OutGraph = FindRegion(Regions, Coordinate);
			OutError.Reset();
			return OutGraph ? EVoxelHydrologyClosureRead::Ready : EVoxelHydrologyClosureRead::Pending;
		};
		const auto& SeedGraph = Regions[0].Graph;
		const int32 SeedAnchor = SeedGraph.FindComponentAnchor(FIntPoint(-6, -6));
		if (!TestTrue(TEXT("查询格定位真实干锚点"), SeedGraph.Nodes.IsValidIndex(SeedAnchor))) return false;
		FVoxelHydrologyNetworkSearch Search;
		if (!Search.Initialize(SeedGraph.Nodes[SeedAnchor].Cell, 32, Side, Generation, Error) || !Finish(Search, Read, 127, Error))
		{
			AddError(Error);
			return false;
		}
		const auto& Result = Search.GetResult();
		TestEqual(TEXT("完成后只保留发布正文，不残留原搜索表"), Search.GetAllocatedBytes(), Result.GetAllocatedBytes());
		TestEqual(TEXT("完成后分页节点已释放"), Search.GetNodeCount(), 0);
		TestEqual(TEXT("完成后区域元数据已释放"), Search.GetRegionCount(), 0);
		for (const auto& Cell : Result.Cells)
		{
			const int32 Dense = Index(Cell.Cell);
			if (!TestEqual(TEXT("跨区最低水位等于逐格展开图"), Cell.Potential.SpillZ, Expanded.Potentials[Dense].SpillZ) ||
				!TestEqual(TEXT("平台距离使用实际边长，而非一条边计一步"), Cell.Potential.PlateauDistance, Expanded.Potentials[Dense].PlateauDistance)) return false;
			if (!TestTrue(TEXT("发布索引能够找回原格控制"), Result.Find(Cell.Cell) == &Cell)) return false;
			if (Expanded.Wet[Dense])
			{
				TestFalse(TEXT("湿根不能穿越到另一片干地"), Cell.bHasParent);
			}
			else
			{
				if (!TestTrue(TEXT("父方向等于真实相邻的逐格父节点"), Cell.bHasParent && Cell.ParentCell == Expanded.Position(Expanded.Parents[Dense]))) return false;
				++CheckedParents;
			}
		}
		for (const auto& Region : Regions)
		{
			for (int32 EdgeIndex = 0; EdgeIndex < Region.Graph.Edges.Num(); ++EdgeIndex)
			{
				const auto& Edge = Region.Graph.Edges[EdgeIndex];
				const auto* First = Result.Find(Region.Graph.Nodes[Edge.First].Cell);
				const auto* Second = Result.Find(Region.Graph.Nodes[Edge.Second].Cell);
				if (!First || !Second || Edge.BarrierZ > Result.Threshold) continue;
				for (int32 Offset = 0; Offset < Edge.ProfileCount; ++Offset)
				{
					FVoxelHydrologyRegionRoute Route;
					if (!Region.Graph.ResolveProfile(EdgeIndex, Offset, First->Potential, Second->Potential, Route, Error))
					{
						AddError(Error);
						return false;
					}
					const int32 Dense = Index(Region.Graph.GetProfileCell(Edge.ProfileOffset + Offset));
					if (!TestTrue(TEXT("网络端点势值可重建完整边内水位、距离与父格"), Route.Potential == Expanded.Potentials[Dense] &&
						Route.ParentCell == Expanded.Position(Expanded.Parents[Dense]))) return false;
					++CheckedProfiles;
				}
			}
		}
	}
	TestTrue(TEXT("实际核对多个跨区控制和内部路径"), CheckedParents > 100 && CheckedProfiles > 100);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyNetworkResumeTest,
	"WHFramework.Voxel.Hydrology.Network.ResumeAndFrozenRegions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyNetworkResumeTest::RunTest(const FString& Parameters)
{
	using namespace VoxelHydrologyNetworkSearchTests;
	(void)Parameters;
	FString Error;
	TArray<FRegion> Regions;
	if (!MakeRegions(Regions, 7, Error))
	{
		AddError(Error);
		return false;
	}
	const auto& Graph = Regions[0].Graph;
	const FIntPoint Seed = Graph.Nodes[Graph.FindComponentAnchor(FIntPoint(-6, -6))].Cell;
	bool bBlock = true;
	int32 Reads = 0;
	const auto Read = [&](const FIntPoint& Coordinate, const FVoxelHydrologyRegionGraph*& OutGraph, FString& OutError)
	{
		++Reads;
		OutGraph = FindRegion(Regions, Coordinate);
		OutError.Reset();
		return OutGraph && (!bBlock || Coordinate == FIntPoint(-1, -1)) ? EVoxelHydrologyClosureRead::Ready : EVoxelHydrologyClosureRead::Pending;
	};
	FVoxelHydrologyNetworkSearch First;
	if (!First.Initialize(Seed, 32, Side, Generation, Error)) return false;
	TAtomic<bool> Cancel(true);
	TestTrue(TEXT("零预算首先响应取消"), First.Advance(Generation, Read, 0, Error, &Cancel) == EVoxelHydrologyNetworkStep::Canceled);
	TestEqual(TEXT("取消不读取区域"), Reads, 0);
	EVoxelHydrologyNetworkStep Step = EVoxelHydrologyNetworkStep::None;
	for (int32 Attempt = 0; Attempt < 10000; ++Attempt)
	{
		Step = First.Advance(Generation, Read, 17, Error);
		if (Step != EVoxelHydrologyNetworkStep::Yielded) break;
	}
	if (!TestTrue(TEXT("未知真实邻区保持等待，不能当作开放出口"), Step == EVoxelHydrologyNetworkStep::NeedInput)) return false;
	TestTrue(TEXT("等待时保留既有工作，不伪造完成"), First.GetNodeCount() > 0 && First.GetAllocatedBytes() > 0);
	TestTrue(TEXT("同一缺失邻区再次等待"), First.Advance(Generation, Read, 17, Error) == EVoxelHydrologyNetworkStep::NeedInput);
	bBlock = false;
	if (!Finish(First, Read, 1, Error))
	{
		AddError(Error);
		return false;
	}
	FVoxelHydrologyNetworkSearch Second;
	if (!Second.Initialize(Seed, 32, Side, Generation, Error) || !Finish(Second, Read, 137, Error))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("缺输入和不同工作预算的完整结果一致"), First.GetResult().HasEquivalentOutput(Second.GetResult()));
	TestTrue(TEXT("拒绝换代复用"), First.Advance(Generation + 1, Read, 1, Error) == EVoxelHydrologyNetworkStep::Failed);
	FVoxelHydrologyNetworkSearch Changed;
	if (!Changed.Initialize(Seed, 32, Side, Generation, Error)) return false;
	TestTrue(TEXT("先接纳真实冻结区域"), Changed.Advance(Generation, Read, 1, Error) == EVoxelHydrologyNetworkStep::Yielded);
	++Regions[0].Graph.Fingerprint;
	TestTrue(TEXT("同代区域被改写后拒绝继续求解"), Changed.Advance(Generation, Read, 10, Error) == EVoxelHydrologyNetworkStep::Failed);
	--Regions[0].Graph.Fingerprint;
	auto Result = First.TakeResult();
	TestTrue(TEXT("正文移交后不残留第二份结果"), First.GetAllocatedBytes() == 0 && !Result.Cells.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyNetworkLedgerTest,
	"WHFramework.Voxel.Hydrology.Network.OriginalLedgerAndRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyNetworkLedgerTest::RunTest(const FString& Parameters)
{
	using namespace VoxelHydrologyNetworkSearchTests;
	(void)Parameters;
	FString Error;
	TArray<FRegion> Regions;
	if (!MakeRegions(Regions, 7, Error))
	{
		AddError(Error);
		return false;
	}
	uint64 SourceBytes = VoxelGenerationAllocation::AllocatedBytes(Regions);
	for (const auto& Region : Regions) SourceBytes += Region.Graph.GetAllocatedBytes();
	FVoxelTerrainResourceLimits Limits;
	Limits.InputBytes = 4ull * 1024 * 1024;
	Limits.WorkingBytes = 4ull * 1024 * 1024;
	Limits.ResidentBytes = 8ull * 1024 * 1024;
	Limits.OverlapBytes = 1ull * 1024 * 1024;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(Generation, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Input;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Working;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Resident;
	const uint64 RootBytes = VoxelBoundedAllocation::SharedBytes<FVoxelHydrologyNetworkSearch>();
	if (!TestTrue(TEXT("原账本预付真实输入、控制对象和正文容量"),
		FVoxelTerrainResources::Create(Limits, Consumer, Resources, Error) &&
		Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, SourceBytes, Input) &&
		Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, RootBytes, Working) &&
		Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident, RootBytes, Resident))) return false;
	TSharedPtr<FVoxelHydrologyNetworkSearch, ESPMode::ThreadSafe> Search = MakeShared<FVoxelHydrologyNetworkSearch, ESPMode::ThreadSafe>();
	const auto& Graph = Regions[0].Graph;
	const FIntPoint Seed = Graph.Nodes[Graph.FindComponentAnchor(FIntPoint(-6, -6))].Cell;
	uint64 SampledWorkingPeak = 0;
	int32 SampledNodePeak = 0;
	{
		FVoxelGenerationRequiredSourceScope SourceScope(Resources.Get(), Input.Get());
		FVoxelGenerationAllocationScope Allocation(Resources, Consumer, Working, Resident, RootBytes);
		if (!TestTrue(TEXT("使用原分配上下文启动网络"), Allocation.Check(Error) && Search->Initialize(Seed, 32, Side, Generation, Error))) return false;
		const auto Read = [&](const FIntPoint& Coordinate, const FVoxelHydrologyRegionGraph*& OutGraph, FString& OutError)
		{
			OutGraph = FindRegion(Regions, Coordinate);
			OutError.Reset();
			return OutGraph ? EVoxelHydrologyClosureRead::Ready : EVoxelHydrologyClosureRead::Pending;
		};
		EVoxelHydrologyNetworkStep Step = EVoxelHydrologyNetworkStep::None;
		for (int32 Attempt = 0; Attempt < 10000; ++Attempt)
		{
			Step = Search->Advance(Generation, Read, 37, Error);
			SampledWorkingPeak = FMath::Max(SampledWorkingPeak, Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)]);
			SampledNodePeak = FMath::Max(SampledNodePeak, Search->GetNodeCount());
			if (!TestTrue(TEXT("全部暂停点按真实backing计量"), Allocation.ValidateRetainedBytes(Search->GetAllocatedBytes(), Error))) return false;
			if (Step == EVoxelHydrologyNetworkStep::Ready) break;
			if (Step != EVoxelHydrologyNetworkStep::Yielded)
			{
				AddError(Error);
				return false;
			}
		}
		if (!TestTrue(TEXT("原账本下完成跨区证书"), Step == EVoxelHydrologyNetworkStep::Ready)) return false;
		TestEqual(TEXT("Ready仅占结果正文及原控制对象"), Resident->Bytes(), RootBytes + Search->GetResult().GetAllocatedBytes());
		TestEqual(TEXT("输入引用未冒领为额外正文"), Input->Bytes(), SourceBytes);
		AddInfo(FString::Printf(TEXT("Network fixture: rawCells=%d sampledNodes=%d controls=%d resultBytes=%llu sampledWorkingHighWater=%llu"),
			Width * Width, SampledNodePeak, Search->GetResult().Cells.Num(), Search->GetResult().GetAllocatedBytes(), SampledWorkingPeak));
	}
	Search.Reset();
	Regions.Empty();
	Input.Reset();
	Working.Reset();
	Resident.Reset();
	const auto End = Resources->Snapshot();
	TestEqual(TEXT("输入和任务退出后不遗留任何租约"), End.LeaseCount, 0);
	TestEqual(TEXT("输入正文全数归还，只剩真实账本自身容量"), End.Used[int32(EVoxelTerrainResourcePool::Input)], End.RootInputBytes + End.LedgerMetadataBytes);
	TestEqual(TEXT("不存在残留租约控制器"), End.LeaseHandleBytes, uint64(0));
	TestEqual(TEXT("原工作额度归还"), End.Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
	TestEqual(TEXT("原驻留额度归还"), End.Used[int32(EVoxelTerrainResourcePool::Resident)], uint64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyNetworkNoReceiverTest,
	"WHFramework.Voxel.Hydrology.Network.PhysicalWorldWithoutReceiver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyNetworkNoReceiverTest::RunTest(const FString& Parameters)
{
	using namespace VoxelHydrologyNetworkSearchTests;
	(void)Parameters;
	FString Error;
	TArray<FRegion> Regions;
	for (int32 Y = -2; Y <= 0; ++Y)
	{
		for (int32 X = -2; X <= 0; ++X)
		{
			FRegion& Region = Regions.AddDefaulted_GetRef();
			Region.Coordinate = FIntPoint(X, Y);
			FVoxelHydrologyRegionGraphBuild Build;
			if (!Build.Initialize(Region.Coordinate, 1, 1, MAX_int32, Generation, Error)) return false;
			const auto Read = [](const FIntPoint&, FVoxelHydrologyRegionSample& OutCell, FString& OutError)
			{
				OutCell.GroundZ = 8;
				OutCell.RainWeight = 1;
				OutError.Reset();
				return EVoxelHydrologyClosureRead::Ready;
			};
			EVoxelHydrologyRegionStep Step = EVoxelHydrologyRegionStep::None;
			for (int32 Attempt = 0; Attempt < 1000; ++Attempt)
			{
				Step = Build.Advance(Generation, Read, 7, Error);
				if (Step != EVoxelHydrologyRegionStep::Yielded) break;
			}
			if (Step != EVoxelHydrologyRegionStep::Ready) return false;
			Region.Graph = Build.TakeResult();
		}
	}
	bool bMissing = true;
	int32 OutsideReads = 0;
	const auto Read = [&](const FIntPoint& Coordinate, const FVoxelHydrologyRegionGraph*& OutGraph, FString& OutError)
	{
		OutGraph = FindRegion(Regions, Coordinate);
		if (!OutGraph) ++OutsideReads;
		OutError.Reset();
		return OutGraph && (!bMissing || Coordinate != FIntPoint(0, 0)) ? EVoxelHydrologyClosureRead::Ready : EVoxelHydrologyClosureRead::Pending;
	};
	FVoxelHydrologyNetworkSearch Search;
	if (!Search.Initialize(FIntPoint(-1, -1), MAX_int32, 1, Generation, Error)) return false;
	EVoxelHydrologyNetworkStep Step = EVoxelHydrologyNetworkStep::None;
	for (int32 Attempt = 0; Attempt < 1000; ++Attempt)
	{
		Step = Search.Advance(Generation, Read, 13, Error);
		if (Step != EVoxelHydrologyNetworkStep::Yielded) break;
	}
	TestTrue(TEXT("少一片真实世界区域也不能伪造出口或宣称无水根"), Step == EVoxelHydrologyNetworkStep::NeedInput);
	bMissing = false;
	for (int32 Attempt = 0; Attempt < 1000; ++Attempt)
	{
		Step = Search.Advance(Generation, Read, 13, Error);
		if (Step != EVoxelHydrologyNetworkStep::Yielded) break;
	}
	TestTrue(TEXT("覆盖实际物理世界后明确报告无湿根，而非伪造水位"), Step == EVoxelHydrologyNetworkStep::NoReceiver);
	TestEqual(TEXT("真实物理边界保留最小端部分粗格，共九个非空交集"), Search.GetNodeCount(), 9);
	TestEqual(TEXT("不越过物理世界边界读取虚构区域"), OutsideReads, 0);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyMacroPipelineConservationTest,
    "WHFramework.Voxel.Hydrology.Macro.RegionalPipelineAgainstExpandedForest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyMacroPipelineConservationTest::RunTest(const FString& Parameters)
{
    using namespace VoxelHydrologyNetworkSearchTests;
    (void)Parameters;
    int32 ComparedCells = 0;
    int32 ComparedChannels = 0;
    int32 ComparedInlets = 0;
    for (int32 Variant = 0; Variant < 8; ++Variant)
    {
        FVoxelGenerationRuntimeConfig Author = *VoxelTest::MakeGenerationConfig();
        FVoxelGenerationRecipe Recipe = *Author.Recipe;
        Recipe.Settings.HydrologyCellSize = 32;
        Recipe.Settings.HydrologyRegionSide = Side;
        Recipe.Settings.HydrologyHaloCells = 1;
        Recipe.Settings.SeaLevel = 0;
        Recipe.Settings.RiverSourceAccumulation = 12;
        Recipe.Settings.RiverSourceSpacing = 4;
        Author.Recipe = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe));
        const auto Config = MakeShared<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe>(MoveTemp(Author));
        FVoxelGenerationPlanCache Cache;
        TMap<FVoxelHydrologyRegionKey, TSharedPtr<FVoxelHydrologyPlan, ESPMode::ThreadSafe>> Topology;
        const auto Lookup = [&](const FVoxelHydrologyRegionKey& InKey) -> const FVoxelHydrologyPlan*
        {
            if (const auto* Existing = Topology.Find(InKey)) return Existing->Get();
            auto Plan = MakeShared<FVoxelHydrologyPlan, ESPMode::ThreadSafe>();
            Plan->Grid.WorldMinCell = InKey.Coordinate * Side;
            Plan->Grid.Width = Side;
            Plan->Grid.Height = Side;
            Plan->Grid.CellSize = 32;
            Plan->Grid.GroundPlane.SetNum(Side * Side);
            Plan->Grid.RainWeight.Init(2, Side * Side);
            for (int32 CellIndex = 0; CellIndex < Side * Side; ++CellIndex)
            {
                const FIntPoint Local = Plan->Grid.ToWorldHydrologyCell(CellIndex) - Minimum;
                const bool bWet = Local.X <= 0 || Local.Y <= 0 || Local.X + 1 >= Width || Local.Y + 1 >= Width;
                Plan->Grid.GroundPlane[CellIndex] = bWet ? -9 : Variant < 6 ? (Local.X * 13 + Local.Y * 7 + Variant * 11) % 17 - 5 : 8;
                if (bWet) Plan->Grid.OceanOutlets.Add(CellIndex, 0);
            }
            const auto* Result = Plan.Get();
            Topology.Add(InKey, MoveTemp(Plan));
            return Result;
        };
        const auto Request = [&](const FVoxelHydrologyRegionKey&, FString& Error)
        {
            Error = TEXT("Pipeline fixture unexpectedly missed its synthetic topology");
            return false;
        };
        FString Error;
        TArray<FRegion> Regions;
        for (int32 Y = -1; Y <= 0; ++Y)
        {
            for (int32 X = -1; X <= 0; ++X)
            {
                FVoxelHydrologyRegionGraphPtr Graph;
                bool bFinished = false;
                for (int32 Attempt = 0; Attempt < 10000; ++Attempt)
                {
                    if (Cache.AdvanceHydrologyGraph({FIntPoint(X, Y)}, Config, Lookup, Request, Graph, Error)) { bFinished = true; break; }
                    if (!Error.StartsWith(TEXT("Yielded"))) { AddError(Error); return false; }
                }
                if (!TestTrue(TEXT("正式缓存入口发布实际区域摘要"), bFinished)) return false;
                FRegion& Region = Regions.AddDefaulted_GetRef();
                Region.Coordinate = FIntPoint(X, Y);
                Region.Graph = *Graph;
            }
        }
        FExpanded Dense;
        if (!TestTrue(TEXT("独立展开完整森林，包含剥枝格而不调用新汇水算法"), Dense.Solve(Regions, true, Variant))) return false;
        constexpr int32 Count = Width * Width;
        TArray<uint64> Accumulation;
        TArray<uint64> MaxChild;
        TArray<int32> Children;
        TArray<int32> Queue;
        Accumulation.Init(0, Count);
        MaxChild.Init(0, Count);
        Children.Init(0, Count);
        for (int32 Cell = 0; Cell < Count; ++Cell)
        {
            Accumulation[Cell] = Dense.Wet[Cell] ? 0 : 2;
            if (Dense.Parents[Cell] != INDEX_NONE) ++Children[Dense.Parents[Cell]];
        }
        for (int32 Cell = 0; Cell < Count; ++Cell) if (!Children[Cell]) Queue.Add(Cell);
        for (int32 Cursor = 0; Cursor < Queue.Num(); ++Cursor)
        {
            const int32 Cell = Queue[Cursor];
            const int32 Parent = Dense.Parents[Cell];
            if (Parent == INDEX_NONE) continue;
            Accumulation[Parent] += Accumulation[Cell];
            MaxChild[Parent] = FMath::Max(MaxChild[Parent], Accumulation[Cell]);
            if (--Children[Parent] == 0) Queue.Add(Parent);
        }
        if (!TestEqual(TEXT("独立汇水父图无环且全部处理"), Queue.Num(), Count)) return false;
        uint64 Received = 0;
        for (int32 Cell = 0; Cell < Count; ++Cell) if (Dense.Wet[Cell]) Received += Accumulation[Cell];
        if (!TestEqual(TEXT("完整逐格总雨量真实进入湿接收体"), Received, uint64((Width - 2) * (Width - 2) * 2))) return false;
        TMap<FIntPoint, int32> BucketWinners;
        const auto SourceId = [&](int32 Cell)
        {
            const auto Position = Dense.Position(Cell) * 32;
            return VoxelGeneration::MakeStableId(Config->Recipe->Settings.Seed, FIntVector(Position.X, Position.Y, 0), 0x5249564552535243ull, 0);
        };
        for (int32 Cell = 0; Cell < Count; ++Cell)
        {
            if (Dense.Wet[Cell] || Accumulation[Cell] < 12 || MaxChild[Cell] >= 12) continue;
            const auto Position = Dense.Position(Cell);
            const FIntPoint Bucket(VoxelGeneration::FloorDivide(Position.X, 4), VoxelGeneration::FloorDivide(Position.Y, 4));
            const int32* Existing = BucketWinners.Find(Bucket);
            if (!Existing || Accumulation[Cell] > Accumulation[*Existing] ||
                (Accumulation[Cell] == Accumulation[*Existing] && SourceId(Cell) < SourceId(*Existing))) BucketWinners.Add(Bucket, Cell);
        }
        TArray<uint8> Active;
        Active.Init(0, Count);
        for (const auto& Winner : BucketWinners) Active[Winner.Value] = 1;
        for (const int32 Cell : Queue) if (Dense.Parents[Cell] != INDEX_NONE) Active[Dense.Parents[Cell]] |= Active[Cell];
        for (const auto& Region : Regions)
        {
            TSharedPtr<const FVoxelHydrologyNetworkRunoff, ESPMode::ThreadSafe> Runoff;
            bool bFinished = false;
            for (int32 Attempt = 0; Attempt < 200000; ++Attempt)
            {
                if (Cache.AdvanceHydrologyRunoff({Region.Coordinate}, Config, Lookup, Request, Runoff, Error)) { bFinished = true; break; }
                if (!Error.StartsWith(TEXT("Yielded"))) { AddError(Error); return false; }
            }
            if (!TestTrue(TEXT("正式新缓存汇水链有界完成"), bFinished)) { AddError(Error); return false; }
            FVoxelHydrologyLocalRunoffBuild LocalBuild;
            if (!LocalBuild.Initialize(Region.Graph, Error)) { AddError(Error); return false; }
            const auto ReadCell = [&](const FIntPoint& Cell, FVoxelHydrologyRegionSample& Sample, FString& ReadError)
            {
                FIntPoint World;
                return FVoxelHydrologyMacroInput::Read(Config->Recipe->Settings, Cell, Lookup, Request, Sample, World, ReadError);
            };
            EVoxelHydrologyRegionStep Step = EVoxelHydrologyRegionStep::None;
            for (int32 Attempt = 0; Attempt < 100000; ++Attempt)
            {
                Step = LocalBuild.Advance(Region.Graph, *Runoff, ReadCell, Variant % 2 ? 1 : 97, Error);
                if (Step != EVoxelHydrologyRegionStep::Yielded) break;
            }
            if (!TestTrue(TEXT("局部森林消费正式跨区边界汇水并完成"), Step == EVoxelHydrologyRegionStep::Ready)) { AddError(Error); return false; }
            const auto& Local = LocalBuild.GetResult();
            if (!TestEqual(TEXT("区域真实雨量加入境等于接收加出境"), Local.LocalRainWeight + Local.IncomingRainWeight, Local.ReceivedRainWeight + Local.ExportedRainWeight)) return false;
            for (int32 CellIndex = 0; CellIndex < Side * Side; ++CellIndex)
            {
                const FIntPoint Cell = Region.Graph.Minimum + FIntPoint(CellIndex % Side, CellIndex / Side);
                const int32 Expected = Index(Cell);
                const auto& Actual = Local.Cells[CellIndex];
                FVoxelHydrologySourceCell Source;
                if (!Runoff->SampleSourceCell(Region.Graph, Cell, Source, Error)) { AddError(Error); return false; }
                if (!TestEqual(TEXT("每个宏观格汇水等于独立逐格结果"), Source.Accumulation, Accumulation[Expected]) ||
                    !TestEqual(TEXT("河源最大子流精确而非近似阈值"), Source.MaxChildRainWeight, MaxChild[Expected]) ||
                    !TestEqual(TEXT("局部汇水与独立逐格结果相同"), Actual.Accumulation, Accumulation[Expected]) ||
                    !TestTrue(TEXT("局部水位/平台距离遵守独立全图控制"), Actual.Potential == Dense.Potentials[Expected]) ||
                    !TestTrue(TEXT("局部父方向与全图独立解一致"), Actual.bHasParent == (Dense.Parents[Expected] != INDEX_NONE) &&
                        (!Actual.bHasParent || Actual.ParentCell == Dense.Position(Dense.Parents[Expected])))) return false;
                ++ComparedCells;
            }
            TSharedPtr<const FVoxelHydrologySourcePlan, ESPMode::ThreadSafe> Sources;
            bFinished = false;
            for (int32 Attempt = 0; Attempt < 200000; ++Attempt)
            {
                if (Cache.AdvanceHydrologySources({Region.Coordinate}, Config, Lookup, Request, Sources, Error)) { bFinished = true; break; }
                if (!Error.StartsWith(TEXT("Yielded"))) { AddError(Error); return false; }
            }
            if (!TestTrue(TEXT("全局固定间距桶竞争在正式缓存完成"), bFinished)) { AddError(Error); return false; }
            int32 ExpectedSources = 0;
            for (const auto& Winner : BucketWinners)
            {
                const auto Cell = Dense.Position(Winner.Value);
                if (VoxelGeneration::FloorDivide(Cell.X, Side) == Region.Coordinate.X && VoxelGeneration::FloorDivide(Cell.Y, Side) == Region.Coordinate.Y)
                {
                    ++ExpectedSources;
                    if (!TestTrue(TEXT("选源包含独立桶竞争胜者并保留实际身份"), Sources->Sources.ContainsByPredicate([&](const auto& Source) { return Source.Cell == Cell && Source.Id == SourceId(Winner.Value); }))) return false;
                }
            }
            if (!TestEqual(TEXT("没有省略或额外河源"), Sources->Sources.Num(), ExpectedSources)) return false;
            TSharedPtr<const FVoxelHydrologyChannelPlan, ESPMode::ThreadSafe> Channels;
            bFinished = false;
            for (int32 Attempt = 0; Attempt < 300000; ++Attempt)
            {
                if (Cache.AdvanceHydrologyChannels({Region.Coordinate}, Config, Lookup, Request, Channels, Error)) { bFinished = true; break; }
                if (!Error.StartsWith(TEXT("Yielded"))) { AddError(Error); return false; }
            }
            if (!TestTrue(TEXT("合法选源河道跨区传播有界完成"), bFinished)) { AddError(Error); return false; }
            for (const auto& Channel : Channels->Cells)
            {
                const int32 Expected = Index(Channel.Cell);
                bool bIncoming = false;
                for (int32 Child = 0; Child < Count; ++Child)
                {
                    if (Dense.Parents[Child] != Expected || !Active[Child]) continue;
                    const auto Position = Dense.Position(Child);
                    if (VoxelGeneration::FloorDivide(Position.X, Side) != Region.Coordinate.X || VoxelGeneration::FloorDivide(Position.Y, Side) != Region.Coordinate.Y) bIncoming = true;
                }
                if (!TestEqual(TEXT("摘要河道激活与独立全图源头传播相同"), Channel.bActive, Active[Expected] != 0) ||
                    !TestEqual(TEXT("入境河道只来自实际跨区上游而非水量阈值"), Channel.bIncomingActive, bIncoming)) return false;
                ++ComparedChannels;
                if (bIncoming) ++ComparedInlets;
            }
        }
        Cache.Reset();
    }
    TestEqual(TEXT("八组起伏/平台全部原格完成独立对照"), ComparedCells, 8 * Width * Width);
    TestTrue(TEXT("实际覆盖跨区摘要控制"), ComparedChannels > 0);
    TestTrue(TEXT("夹具必须真实产生跨区入境河道"), ComparedInlets > 0);
    return true;
}

#endif
