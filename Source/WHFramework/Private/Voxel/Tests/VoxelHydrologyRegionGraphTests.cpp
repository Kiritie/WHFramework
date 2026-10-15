#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyRegionGraph.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

namespace VoxelHydrologyRegionGraphTests
{
	TArray<int32> DenseCosts(const TArray<FVoxelHydrologyRegionSample>& InCells, const int32 InWidth, const int32 InSource)
	{
		TArray<int32> Costs;
		TArray<uint8> Visited;
		Costs.Init(MAX_int32, InCells.Num());
		Visited.Init(0, InCells.Num());
		Costs[InSource] = InCells[InSource].IsReceiver() ? InCells[InSource].WaterZ : InCells[InSource].GroundZ;
		for (int32 Work = 0; Work < InCells.Num(); ++Work)
		{
			int32 Current = INDEX_NONE;
			for (int32 Index = 0; Index < Costs.Num(); ++Index)
			{
				if (!Visited[Index] && Costs[Index] != MAX_int32 &&
					(Current == INDEX_NONE || Costs[Index] < Costs[Current])) Current = Index;
			}
			if (Current == INDEX_NONE) break;
			Visited[Current] = 1;
			if (InCells[Current].IsReceiver()) continue;
			const int32 Neighbors[] = {Current % InWidth > 0 ? Current - 1 : INDEX_NONE,
				Current % InWidth + 1 < InWidth ? Current + 1 : INDEX_NONE,
				Current >= InWidth ? Current - InWidth : INDEX_NONE,
				Current + InWidth < InCells.Num() ? Current + InWidth : INDEX_NONE};
			for (const int32 Neighbor : Neighbors)
			{
				if (Neighbor == INDEX_NONE) continue;
				const auto& Cell = InCells[Neighbor];
				const int32 Plane = Cell.IsReceiver() ? Cell.WaterZ : Cell.GroundZ;
				Costs[Neighbor] = FMath::Min(Costs[Neighbor], FMath::Max(Costs[Current], Plane));
			}
		}
		return Costs;
	}

	TArray<int32> SummaryCosts(const FVoxelHydrologyRegionGraph& InGraph, const int32 InSource)
	{
		TArray<int32> Costs;
		TArray<uint8> Visited;
		Costs.Init(MAX_int32, InGraph.Nodes.Num());
		Visited.Init(0, InGraph.Nodes.Num());
		const auto& Source = InGraph.Nodes[InSource];
		Costs[InSource] = Source.IsReceiver() ? Source.WaterZ : Source.GroundZ;
		for (int32 Work = 0; Work < Costs.Num(); ++Work)
		{
			int32 Current = INDEX_NONE;
			for (int32 Index = 0; Index < Costs.Num(); ++Index)
			{
				if (!Visited[Index] && Costs[Index] != MAX_int32 &&
					(Current == INDEX_NONE || Costs[Index] < Costs[Current])) Current = Index;
			}
			if (Current == INDEX_NONE) break;
			Visited[Current] = 1;
			if (InGraph.Nodes[Current].IsReceiver()) continue;
			for (const auto& Edge : InGraph.Edges)
			{
				const int32 Neighbor = Edge.First == Current ? Edge.Second : Edge.Second == Current ? Edge.First : INDEX_NONE;
				if (Neighbor != INDEX_NONE) Costs[Neighbor] = FMath::Min(Costs[Neighbor], FMath::Max(Costs[Current], Edge.BarrierZ));
			}
		}
		return Costs;
	}

	bool Finish(FVoxelHydrologyRegionGraphBuild& InBuild, const uint64 InGeneration,
		FVoxelHydrologyRegionGraphBuild::FReadCell InRead, const int32 InBudget, FString& OutError)
	{
		for (int32 Attempt = 0; Attempt < 100000; ++Attempt)
		{
			const auto Step = InBuild.Advance(InGeneration, InRead, InBudget, OutError);
			if (Step == EVoxelHydrologyRegionStep::Ready) return true;
			if (Step != EVoxelHydrologyRegionStep::Yielded && Step != EVoxelHydrologyRegionStep::NeedInput) return false;
		}
		OutError = TEXT("Region graph fixture did not finish within its finite work bound");
		return false;
	}

	bool CellLess(const FIntPoint& InFirst, const FIntPoint& InSecond)
	{
		return InFirst.X < InSecond.X || (InFirst.X == InSecond.X && InFirst.Y < InSecond.Y);
	}

	bool ExpandedRoutes(const FVoxelHydrologyRegionGraph& InGraph, TArray<FVoxelHydrologyRegionPotential>& OutPotentials,
		TArray<int32>& OutParents, TArray<FIntPoint>& OutPositions)
	{
		const int32 Count = InGraph.Nodes.Num() + InGraph.Profiles.Num();
		TArray<TArray<int32>> Neighbors;
		TArray<int32> Ground;
		TArray<uint8> Wet;
		TArray<uint8> Visited;
		Neighbors.SetNum(Count);
		Ground.SetNum(Count);
		Wet.Init(0, Count);
		Visited.Init(0, Count);
		OutPotentials.SetNum(Count);
		OutParents.Init(INDEX_NONE, Count);
		OutPositions.SetNum(Count);
		for (int32 Index = 0; Index < InGraph.Nodes.Num(); ++Index)
		{
			const auto& Node = InGraph.Nodes[Index];
			Ground[Index] = Node.GroundZ;
			OutPositions[Index] = Node.Cell;
			if (Node.IsReceiver())
			{
				Wet[Index] = 1;
				OutPotentials[Index] = {Node.WaterZ, 0};
			}
		}
		for (int32 Index = 0; Index < InGraph.Profiles.Num(); ++Index)
		{
			Ground[InGraph.Nodes.Num() + Index] = InGraph.Profiles[Index].GroundZ;
			OutPositions[InGraph.Nodes.Num() + Index] = InGraph.GetProfileCell(Index);
		}
		for (const auto& Edge : InGraph.Edges)
		{
			int32 Previous = Edge.First;
			for (int32 Offset = 0; Offset <= Edge.ProfileCount; ++Offset)
			{
				const int32 Current = Offset < Edge.ProfileCount ? InGraph.Nodes.Num() + Edge.ProfileOffset + Offset : Edge.Second;
				Neighbors[Previous].Add(Current);
				Neighbors[Current].Add(Previous);
				Previous = Current;
			}
		}
		for (int32 Work = 0; Work < Count; ++Work)
		{
			int32 Current = INDEX_NONE;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				if (!Visited[Index] && OutPotentials[Index].SpillZ != MAX_int32 &&
					(Current == INDEX_NONE || OutPotentials[Index].SpillZ < OutPotentials[Current].SpillZ)) Current = Index;
			}
			if (Current == INDEX_NONE) return false;
			Visited[Current] = 1;
			for (const int32 Neighbor : Neighbors[Current])
			{
				if (Wet[Neighbor]) continue;
				OutPotentials[Neighbor].SpillZ = FMath::Min(OutPotentials[Neighbor].SpillZ,
					FMath::Max(OutPotentials[Current].SpillZ, Ground[Neighbor]));
			}
		}
		TArray<int32> Queue;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (Wet[Index])
			{
				Queue.Add(Index);
				continue;
			}
			for (const int32 Neighbor : Neighbors[Index])
			{
				if (OutPotentials[Neighbor].SpillZ >= OutPotentials[Index].SpillZ) continue;
				if (OutParents[Index] == INDEX_NONE || CellLess(OutPositions[Neighbor], OutPositions[OutParents[Index]])) OutParents[Index] = Neighbor;
			}
			if (OutParents[Index] != INDEX_NONE)
			{
				OutPotentials[Index].PlateauDistance = 0;
				Queue.Add(Index);
			}
		}
		for (int32 Cursor = 0; Cursor < Queue.Num(); ++Cursor)
		{
			const int32 Current = Queue[Cursor];
			for (const int32 Neighbor : Neighbors[Current])
			{
				if (OutPotentials[Neighbor].SpillZ != OutPotentials[Current].SpillZ ||
					OutPotentials[Neighbor].PlateauDistance != MAX_int64) continue;
				OutPotentials[Neighbor].PlateauDistance = OutPotentials[Current].PlateauDistance + 1;
				Queue.Add(Neighbor);
			}
		}
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (OutPotentials[Index].PlateauDistance == MAX_int64) return false;
			if (Wet[Index] || OutParents[Index] != INDEX_NONE) continue;
			for (const int32 Neighbor : Neighbors[Index])
			{
				if (OutPotentials[Neighbor].SpillZ != OutPotentials[Index].SpillZ ||
					OutPotentials[Neighbor].PlateauDistance + 1 != OutPotentials[Index].PlateauDistance) continue;
				if (OutParents[Index] == INDEX_NONE || CellLess(OutPositions[Neighbor], OutPositions[OutParents[Index]])) OutParents[Index] = Neighbor;
			}
			if (OutParents[Index] == INDEX_NONE) return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyRegionGraphFlowTest,
	"WHFramework.Voxel.Hydrology.RegionGraph.InteriorFlowAndRainSplit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyRegionGraphFlowTest::RunTest(const FString& Parameters)
{
	using namespace VoxelHydrologyRegionGraphTests;
	(void)Parameters;
	FString Error;
	int32 ProfileChecks = 0;
	int32 SplitEdges = 0;
	for (int32 Variant = 0; Variant < 24; ++Variant)
	{
		const int32 Width = Variant < 20 ? 11 : 33;
		const int32 Height = Variant < 20 ? 9 : 17;
		const FIntPoint Minimum(-23, -19);
		FVoxelHydrologyRegionGraphBuild Build;
		const auto Read = [&](const FIntPoint& Position, FVoxelHydrologyRegionSample& OutCell, FString& OutError)
		{
			const FIntPoint Local = Position - Minimum;
			OutCell.GroundZ = Variant < 20 ? (Local.X * 13 + Local.Y * 7 + Variant * 11) % 17 - 5 : 8;
			OutCell.RainWeight = 1 + (Local.X + Local.Y) % 5;
			if (Local.X == 0 || (Variant % 3 == 0 && Local.X == Width - 1))
			{
				OutCell.GroundZ = -9;
				OutCell.WaterZ = Variant % 2 == 0 ? 0 : 4;
				OutCell.WaterId = {1, uint64(Local.X + 1)};
			}
			OutError.Reset();
			return EVoxelHydrologyClosureRead::Ready;
		};
		if (!Build.Initialize(Minimum, Width, Height, 32, 337, Error) || !Finish(Build, 337, Read, 43, Error))
		{
			AddError(Error);
			return false;
		}
		const auto& Graph = Build.GetResult();
		TArray<FVoxelHydrologyRegionPotential> Potentials;
		TArray<int32> Parents;
		TArray<FIntPoint> Positions;
		if (!TestTrue(TEXT("独立逐格展开图求解真实湿根、最低水位和平台父节点"), ExpandedRoutes(Graph, Potentials, Parents, Positions))) return false;
		for (int32 EdgeIndex = 0; EdgeIndex < Graph.Edges.Num(); ++EdgeIndex)
		{
			const auto& Edge = Graph.Edges[EdgeIndex];
			if (!TestFalse(TEXT("每片干地有真实干锚点，湿根之间不再直接收缩"), Graph.Nodes[Edge.First].IsReceiver() && Graph.Nodes[Edge.Second].IsReceiver())) return false;
			FVoxelHydrologyRegionSplit Split;
			if (!Graph.SplitEdgeRain(EdgeIndex, Potentials[Edge.First], Potentials[Edge.Second], Split, Error))
			{
				AddError(Error);
				return false;
			}
			uint64 FirstRain = 0;
			int32 FirstCount = 0;
			bool bReachedSecond = false;
			for (int32 Offset = 0; Offset < Edge.ProfileCount; ++Offset)
			{
				FVoxelHydrologyRegionRoute Route;
				if (!Graph.ResolveProfile(EdgeIndex, Offset, Potentials[Edge.First], Potentials[Edge.Second], Route, Error))
				{
					AddError(Error);
					return false;
				}
				const int32 Expanded = Graph.Nodes.Num() + Edge.ProfileOffset + Offset;
				if (!TestEqual(TEXT("边内水位等于独立逐格图"), Route.Potential.SpillZ, Potentials[Expanded].SpillZ) ||
					!TestEqual(TEXT("边内平台距离等于独立逐格图"), Route.Potential.PlateauDistance, Potentials[Expanded].PlateauDistance) ||
					!TestTrue(TEXT("边内父格与逐格求解完全一致"), Route.ParentCell == Positions[Parents[Expanded]])) return false;
				if (Route.bTowardFirst)
				{
					if (!TestFalse(TEXT("一条通路内部只有一次分水，不重复反向"), bReachedSecond)) return false;
					++FirstCount;
					const int32 Profile = Edge.ProfileOffset + Offset;
					FirstRain += Graph.Profiles[Profile].RainPrefix - (Offset ? Graph.Profiles[Profile - 1].RainPrefix : 0);
				}
				else bReachedSecond = true;
				++ProfileChecks;
			}
			TestEqual(TEXT("二分求得的分水位置等于逐格父节点归属"), Split.FirstProfileCount, FirstCount);
			TestEqual(TEXT("前端实际收到所属格的雨量"), Split.FirstRainWeight, FirstRain);
			TestEqual(TEXT("两端收到雨量严格等于原边内总雨量"), Split.FirstRainWeight + Split.SecondRainWeight, Edge.InteriorRainWeight);
			if (FirstCount > 0 && FirstCount < Edge.ProfileCount) ++SplitEdges;
		}
		FVoxelHydrologyRegionSplit InvalidSplit;
		TestFalse(TEXT("未解析端点不能被当作真实出口"), Graph.SplitEdgeRain(0, {}, {}, InvalidSplit, Error));
	}
	TestTrue(TEXT("验证包含真实内部路径而非全是零长摘要"), ProfileChecks > 1000);
	TestTrue(TEXT("验证包含实际向两端分流的路径"), SplitEdges > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyRegionGraphMinimaxTest,
	"WHFramework.Voxel.Hydrology.RegionGraph.MinimaxAndRainConservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyRegionGraphMinimaxTest::RunTest(const FString& Parameters)
{
	using namespace VoxelHydrologyRegionGraphTests;
	(void)Parameters;
	constexpr int32 Width = 7;
	constexpr int32 Height = 6;
	constexpr uint64 Generation = 117;
	const FIntPoint Minimum(-19, -11);
	for (int32 Variant = 0; Variant < 14; ++Variant)
	{
		TArray<FVoxelHydrologyRegionSample> Cells;
		Cells.SetNum(Width * Height);
		uint64 RainWeight = 0;
		for (int32 Index = 0; Index < Cells.Num(); ++Index)
		{
			auto& Cell = Cells[Index];
			Cell.GroundZ = (Index * 13 + Index / Width * 7 + Variant * 11) % 17 - 5;
			Cell.RainWeight = 1 + Index % 5;
			if ((Variant < 8 && (Index + Variant) % 5 == 3 && Cell.GroundZ < 8) ||
				Variant == 12 || (Variant == 13 && Index % Width == Width / 2))
			{
				if (Variant >= 12) Cell.GroundZ = 0;
				Cell.WaterZ = 8;
				Cell.WaterId = {1, 2};
			}
			else RainWeight += Cell.RainWeight;
		}
		const auto Read = [&](const FIntPoint& Position, FVoxelHydrologyRegionSample& OutCell, FString& OutError)
		{
			const FIntPoint Local = Position - Minimum;
			OutCell = Cells[Local.X + Local.Y * Width];
			OutError.Reset();
			return EVoxelHydrologyClosureRead::Ready;
		};
		FVoxelHydrologyRegionGraphBuild Build;
		FString Error;
		if (!TestTrue(TEXT("以负坐标真实粗格域初始化摘要"), Build.Initialize(Minimum, Width, Height, 32, Generation, Error)) ||
			!TestTrue(TEXT("分片推进真实摘要构建"), Finish(Build, Generation, Read, 17, Error)))
		{
			AddError(Error);
			return false;
		}
		const auto& Graph = Build.GetResult();
		TestEqual(TEXT("土地雨量不因压缩而丢弃"), Graph.TotalLandRainWeight, RainWeight);
		TestEqual(TEXT("就绪时只留下摘要正文，构建临时容器已释放"), Build.GetAllocatedBytes(), Graph.GetAllocatedBytes());
		uint64 SummaryRain = 0;
		for (const auto& Node : Graph.Nodes) SummaryRain += Node.RainWeight;
		for (const auto& Edge : Graph.Edges)
		{
			SummaryRain += Edge.InteriorRainWeight;
			const auto& First = Graph.Nodes[Edge.First];
			const auto& Second = Graph.Nodes[Edge.Second];
			FIntPoint Previous = First.Cell;
			int32 FirstMaximum = First.IsReceiver() ? First.WaterZ : First.GroundZ;
			for (int32 Offset = 0; Offset < Edge.ProfileCount; ++Offset)
			{
				const int32 ProfileIndex = Edge.ProfileOffset + Offset;
				const FIntPoint Cell = Graph.GetProfileCell(ProfileIndex);
				const FIntPoint Original = Cell - Minimum;
				const int32 Ground = Cells[Original.X + Original.Y * Width].GroundZ;
				FirstMaximum = FMath::Max(FirstMaximum, Ground);
				TestEqual(TEXT("内部通路保持真实四邻格连接"), FMath::Abs(Cell.X - Previous.X) + FMath::Abs(Cell.Y - Previous.Y), 1);
				TestEqual(TEXT("前向障碍来自实际完整路径"), Graph.Profiles[ProfileIndex].FirstBarrierZ, FirstMaximum);
				TestEqual(TEXT("路径高度仍为原始粗格高度"), Graph.Profiles[ProfileIndex].GroundZ, Ground);
				Previous = Cell;
			}
			TestEqual(TEXT("通路末端与真实接收节点相邻"), FMath::Abs(Second.Cell.X - Previous.X) + FMath::Abs(Second.Cell.Y - Previous.Y), 1);
			TestEqual(TEXT("摘要障碍等于完整路径最高障碍"), Edge.BarrierZ,
				FMath::Max(FirstMaximum, Second.IsReceiver() ? Second.WaterZ : Second.GroundZ));
			int32 SecondMaximum = Second.IsReceiver() ? Second.WaterZ : Second.GroundZ;
			for (int32 Offset = Edge.ProfileCount - 1; Offset >= 0; --Offset)
			{
				const auto& Profile = Graph.Profiles[Edge.ProfileOffset + Offset];
				SecondMaximum = FMath::Max(SecondMaximum, Profile.GroundZ);
				TestEqual(TEXT("反向障碍支持沿边确定真实分水"), Profile.SecondBarrierZ, SecondMaximum);
			}
			TestEqual(TEXT("雨量前缀末端包含整条边内部贡献"),
				Edge.ProfileCount ? Graph.Profiles[Edge.ProfileOffset + Edge.ProfileCount - 1].RainPrefix : uint64(0), Edge.InteriorRainWeight);
		}
		TestEqual(TEXT("节点与边内雨量严格守恒"), SummaryRain, RainWeight);
		TestEqual(TEXT("定位区间严格覆盖本区域，不额外扩张"), Graph.AnchorRuns.Last().EndExclusive, uint32(Cells.Num()));
		if (Variant == 12) TestEqual(TEXT("全水区域不保留重复逐格入口"), Graph.AnchorRuns.Num(), 1);
		TestEqual(TEXT("区域外不返回虚构入口"), Graph.FindComponentAnchor(Minimum - FIntPoint(1, 0)), INDEX_NONE);
		for (int32 Index = 0; Index < Cells.Num(); ++Index)
		{
			if (Cells[Index].IsReceiver()) continue;
			const int32 Anchor = Graph.FindComponentAnchor(Minimum + FIntPoint(Index % Width, Index / Width));
			if (!TestTrue(TEXT("任意干格均定位到保留的真实干节点"), Graph.Nodes.IsValidIndex(Anchor) && !Graph.Nodes[Anchor].IsReceiver())) return false;
			const auto Dense = DenseCosts(Cells, Width, Index);
			for (int32 Other = 0; Other < Cells.Num(); ++Other)
			{
				if (Cells[Other].IsReceiver()) continue;
				const int32 OtherAnchor = Graph.FindComponentAnchor(Minimum + FIntPoint(Other % Width, Other / Width));
				if (!TestTrue(TEXT("相同干地分量共享入口，不能跨湿根合并不同干地"),
					(Anchor == OtherAnchor) == (Dense[Other] != MAX_int32))) return false;
			}
		}
		for (int32 Source = 0; Source < Graph.Nodes.Num(); ++Source)
		{
			const FIntPoint Local = Graph.Nodes[Source].Cell - Minimum;
			const auto Dense = DenseCosts(Cells, Width, Local.X + Local.Y * Width);
			const auto Summary = SummaryCosts(Graph, Source);
			for (int32 Target = 0; Target < Graph.Nodes.Num(); ++Target)
			{
				const FIntPoint TargetLocal = Graph.Nodes[Target].Cell - Minimum;
				if (!TestEqual(TEXT("压缩前后全部保留节点的最低越障代价一致，湿根不能当作过路口"),
					Summary[Target], Dense[TargetLocal.X + TargetLocal.Y * Width])) return false;
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyRegionGraphLedgerTest,
	"WHFramework.Voxel.Hydrology.RegionGraph.OriginalLedgerAndReduction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyRegionGraphLedgerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	constexpr int32 Side = 128;
	constexpr uint64 Generation = 233;
	const FIntPoint Minimum(-128, -64);
	const auto Consumer = FVoxelGenerationConsumerKey::Section(233, FGuid::NewGuid(), 1, {}, 1);
	FVoxelTerrainResourceLimits Limits;
	Limits.InputBytes = 4ull * 1024 * 1024;
	Limits.WorkingBytes = 16ull * 1024 * 1024;
	Limits.ResidentBytes = 32ull * 1024 * 1024;
	Limits.OverlapBytes = 1ull * 1024 * 1024;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Working;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Resident;
	FString Error;
	const uint64 RootBytes = VoxelBoundedAllocation::SharedBytes<FVoxelHydrologyRegionGraphBuild>();
	if (!TestTrue(TEXT("原账本先授予真实对象和控制器容量"),
		FVoxelTerrainResources::Create(Limits, Consumer, Resources, Error) &&
		Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, RootBytes, Working) &&
		Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident, RootBytes, Resident))) return false;
	TSharedPtr<FVoxelHydrologyRegionGraphBuild, ESPMode::ThreadSafe> Build = MakeShared<FVoxelHydrologyRegionGraphBuild, ESPMode::ThreadSafe>();
	uint64 SampledWorkingPeak = 0;
	{
		FVoxelGenerationAllocationScope Allocation(Resources, Consumer, Working, Resident, RootBytes);
		if (!TestTrue(TEXT("以原正文授额建立区域"), Allocation.Check(Error) &&
			Build->Initialize(Minimum, Side, Side, 32, Generation, Error))) return false;
		const auto Read = [&](const FIntPoint& Position, FVoxelHydrologyRegionSample& OutCell, FString& OutError)
		{
			OutCell.GroundZ = 8 + (Position.Y - Minimum.Y) / 16;
			OutCell.RainWeight = 5;
			if (Position.X == Minimum.X)
			{
				OutCell.GroundZ = -4;
				OutCell.WaterZ = 0;
				OutCell.WaterId = {9, 11};
			}
			OutError.Reset();
			return EVoxelHydrologyClosureRead::Ready;
		};
		EVoxelHydrologyRegionStep Step = EVoxelHydrologyRegionStep::None;
		for (int32 Attempt = 0; Attempt < 10000; ++Attempt)
		{
			Step = Build->Advance(Generation, Read, 257, Error);
			SampledWorkingPeak = FMath::Max(SampledWorkingPeak, Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)]);
			if (!TestTrue(TEXT("每个暂停点的全部正文都由原授额覆盖"), Allocation.ValidateRetainedBytes(Build->GetAllocatedBytes(), Error))) return false;
			if (Step == EVoxelHydrologyRegionStep::Ready) break;
			if (Step != EVoxelHydrologyRegionStep::Yielded)
			{
				AddError(Error);
				return false;
			}
		}
		if (!TestTrue(TEXT("受控128方形真实区域完成摘要"), Step == EVoxelHydrologyRegionStep::Ready)) return false;
		const auto& Graph = Build->GetResult();
		TestEqual(TEXT("全部陆地源雨量不因压缩减少"), Graph.TotalLandRainWeight, uint64((Side - 1) * Side * 5));
		TestTrue(TEXT("全局参与求解的节点明显少于原粗格"), Graph.Nodes.Num() < Side * Side / 4);
		TestTrue(TEXT("入口查询区间不再逐格重复保存同一分量编号"), Graph.AnchorRuns.Num() < Side * Side / 4);
		TestTrue(TEXT("Ready不继续持有大幅构建工作集"), RootBytes + Graph.GetAllocatedBytes() < SampledWorkingPeak / 2);
		TestEqual(TEXT("只保留实际摘要正文的原驻留额度"), Resident->Bytes(), RootBytes + Graph.GetAllocatedBytes());
		AddInfo(FString::Printf(TEXT("Region summary: cells=%d nodes=%d edges=%d profiles=%d anchorRuns=%d bytes=%llu sampledWorkingHighWater=%llu"),
			Side * Side, Graph.Nodes.Num(), Graph.Edges.Num(), Graph.Profiles.Num(), Graph.AnchorRuns.Num(), Graph.GetAllocatedBytes(), SampledWorkingPeak));
	}
	Build.Reset();
	Working.Reset();
	Resident.Reset();
	const auto End = Resources->Snapshot();
	TestEqual(TEXT("区域退出后所有正文句柄均释放"), End.LeaseCount, 0);
	TestEqual(TEXT("没有遗留工作池占用"), End.Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
	TestEqual(TEXT("没有遗留驻留池占用"), End.Used[int32(EVoxelTerrainResourcePool::Resident)], uint64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyRegionGraphResumeTest,
	"WHFramework.Voxel.Hydrology.RegionGraph.ResumeAndFrozenIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyRegionGraphResumeTest::RunTest(const FString& Parameters)
{
	using namespace VoxelHydrologyRegionGraphTests;
	(void)Parameters;
	constexpr uint64 Generation = 193;
	FString Error;
	FVoxelHydrologyRegionGraphBuild First;
	if (!TestTrue(TEXT("初始化固定区域"), First.Initialize(FIntPoint(-4, -3), 9, 9, 32, Generation, Error))) return false;
	int32 Reads = 0;
	const auto Read = [&](const FIntPoint& Position, FVoxelHydrologyRegionSample& OutCell, FString& OutError)
	{
		++Reads;
		OutCell.GroundZ = 8;
		OutCell.RainWeight = 3;
		if (Position == FIntPoint::ZeroValue)
		{
			OutCell.GroundZ = 0;
			OutCell.WaterZ = 4;
			OutCell.WaterId = {3, 7};
		}
		OutError.Reset();
		return EVoxelHydrologyClosureRead::Ready;
	};
	TAtomic<bool> Cancel(true);
	TestTrue(TEXT("零工作预算也能先处理取消"),
		First.Advance(Generation, Read, 0, Error, &Cancel) == EVoxelHydrologyRegionStep::Canceled);
	TestEqual(TEXT("取消前不读取输入"), Reads, 0);
	Cancel.Store(false);
	bool bPending = true;
	const auto PendingRead = [&](const FIntPoint& Position, FVoxelHydrologyRegionSample& OutCell, FString& OutError)
	{
		if (Position == FIntPoint(-2, -2) && bPending)
		{
			bPending = false;
			return EVoxelHydrologyClosureRead::Pending;
		}
		return Read(Position, OutCell, OutError);
	};
	if (!TestTrue(TEXT("缺输入与逐步续跑完成"), Finish(First, Generation, PendingRead, 1, Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("已读取的格不因续跑重复采样"), Reads, 81);
	FVoxelHydrologyRegionGraphBuild Second;
	if (!Second.Initialize(FIntPoint(-4, -3), 9, 9, 32, Generation, Error) ||
		!Finish(Second, Generation, Read, 67, Error))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("输入冻结指纹不随工作片变化"), First.GetResult().InputFingerprint, Second.GetResult().InputFingerprint);
	TestEqual(TEXT("完整摘要指纹不随工作片或等待变化"), First.GetResult().Fingerprint, Second.GetResult().Fingerprint);
	TestTrue(TEXT("完整结果逐项相等，不只比较哈希"), First.GetResult().HasEquivalentOutput(Second.GetResult()));
	TestTrue(TEXT("典型区域摘要减少实际保留节点"), First.GetResult().Nodes.Num() < 81);
	TestTrue(TEXT("换代输入不得复用旧构建状态"),
		First.Advance(Generation + 1, Read, 10, Error) == EVoxelHydrologyRegionStep::Failed);
	auto Result = First.TakeResult();
	TestTrue(TEXT("摘要移交后原构建正文为空"), First.GetAllocatedBytes() == 0 && !Result.Nodes.IsEmpty());
	return true;
}

#endif
