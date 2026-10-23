#include "Voxel/Generation/Hydrology/VoxelHydrologyNetworkSearch.h"

#include "Voxel/Generation/VoxelGenerationMath.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeExit.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	bool HydrologyNetworkCellLess(const FIntPoint& InFirst, const FIntPoint& InSecond)
	{
		return InFirst.X < InSecond.X || (InFirst.X == InSecond.X && InFirst.Y < InSecond.Y);
	}

	int32 HydrologyNetworkCapacity(const int32 InCount)
	{
		return int32(FMath::Min<int64>(MAX_int32, FMath::Max<int64>(16, int64(InCount) * 2)));
	}
}

const FVoxelHydrologyNetworkCell* FVoxelHydrologyNetworkResult::Find(const FIntPoint& InCell) const
{
	int32 Lower = 0;
	int32 Upper = Cells.Num();
	while (Lower < Upper)
	{
		const int32 Middle = Lower + (Upper - Lower) / 2;
		if (HydrologyNetworkCellLess(Cells[Middle].Cell, InCell))
		{
			Lower = Middle + 1;
		}
		else
		{
			Upper = Middle;
		}
	}
	return Cells.IsValidIndex(Lower) && Cells[Lower].Cell == InCell ? &Cells[Lower] : nullptr;
}

uint64 FVoxelHydrologyNetworkResult::GetAllocatedBytes() const
{
	return VoxelGenerationAllocation::AllocatedBytes(Cells);
}

bool FVoxelHydrologyNetworkResult::HasEquivalentOutput(const FVoxelHydrologyNetworkResult& InOther) const
{
	return Generation == InOther.Generation && InputFingerprint == InOther.InputFingerprint && Fingerprint == InOther.Fingerprint &&
		Threshold == InOther.Threshold && Cells == InOther.Cells;
}

FVoxelHydrologyNetworkSearch::FVoxelHydrologyNetworkSearch()
{
}

FVoxelHydrologyNetworkSearch::~FVoxelHydrologyNetworkSearch()
{
}

bool FVoxelHydrologyNetworkSearch::Initialize(const FIntPoint& InSeedNode, const int32 InCellSize,
	const int32 InRegionSide, const uint64 InGeneration, FString& OutError)
{
	if (Stage != EStage::None || InCellSize <= 0 || InRegionSide <= 0 || InRegionSide > 4096 || !InGeneration)
	{
		OutError = TEXT("Invalid hydrology network generation or dimensions");
		return false;
	}
	const int64 Lower = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMin, InCellSize);
	const int64 Upper = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMaxExclusive - 1, InCellSize);
	if (InSeedNode.X < Lower || InSeedNode.Y < Lower || InSeedNode.X > Upper || InSeedNode.Y > Upper)
	{
		OutError = TEXT("Hydrology network seed is outside the physical world");
		return false;
	}
	Minimum = FIntPoint(int32(Lower), int32(Lower));
	Maximum = FIntPoint(int32(Upper), int32(Upper));
	Seed = InSeedNode;
	CellSize = InCellSize;
	RegionSide = InRegionSide;
	Result.Generation = InGeneration;
	PendingRegion = FIntPoint(VoxelGeneration::FloorDivide(Seed.X, RegionSide), VoxelGeneration::FloorDivide(Seed.Y, RegionSide));
	Stage = EStage::LoadRegion;
	OutError.Reset();
	return true;
}

bool FVoxelHydrologyNetworkSearch::InitializeRegion(const FIntPoint& InSeedNode, const int32 InCellSize,
	const int32 InRegionSide, const uint64 InGeneration, FString& OutError)
{
	if (!Initialize(InSeedNode, InCellSize, InRegionSide, InGeneration, OutError)) return false;
	bEntireRegion = true;
	return true;
}

EVoxelHydrologyClosureRead FVoxelHydrologyNetworkSearch::ReadRegion(const FIntPoint& InCoordinate, FReadRegion InRead,
	int32& OutRegion, const FVoxelHydrologyRegionGraph*& OutGraph, FString& OutError)
{
	++RegionReadRequests;
	OutGraph = nullptr;
	const int32* Existing = ByRegion.Find(InCoordinate);
	if (Existing && Regions[*Existing].SliceGraph)
	{
		++SliceGraphHits;
		OutRegion = *Existing;
		OutGraph = Regions[*Existing].SliceGraph;
		OutError.Reset();
		return EVoxelHydrologyClosureRead::Ready;
	}
	const auto Read = InRead(InCoordinate, OutGraph, OutError);
	if (Read != EVoxelHydrologyClosureRead::Ready)
	{
		return Read;
	}
	if (!OutGraph)
	{
		OutError = TEXT("Hydrology network requires the frozen nonoverlapping physical region core");
		return EVoxelHydrologyClosureRead::Failed;
	}
	if (!OutGraph->ValidateCore(InCoordinate, CellSize, RegionSide, Result.Generation, OutError))
	{
		return EVoxelHydrologyClosureRead::Failed;
	}
	if (Existing)
	{
		auto& Region = Regions[*Existing];
		if (Region.Fingerprint != OutGraph->Fingerprint || Region.InputFingerprint != OutGraph->InputFingerprint)
		{
			OutError = TEXT("Hydrology network region changed after admission");
			return EVoxelHydrologyClosureRead::Failed;
		}
		OutRegion = *Existing;
		Region.SliceGraph = OutGraph;
		return EVoxelHydrologyClosureRead::Ready;
	}
	if (Regions.Num() == MAX_int32)
	{
		OutError = TEXT("Oversized: hydrology network region count");
		return EVoxelHydrologyClosureRead::Failed;
	}
	if (Regions.Num() == Regions.Max() && !VoxelGenerationAllocation::RetainArray(Regions, HydrologyNetworkCapacity(Regions.Num()), OutError))
	{
		return EVoxelHydrologyClosureRead::Failed;
	}
	if (ByRegion.Num() == RegionMapCapacity)
	{
		const int32 Capacity = HydrologyNetworkCapacity(RegionMapCapacity);
		if (!VoxelGenerationAllocation::RetainMap(ByRegion, Capacity, OutError))
		{
			return EVoxelHydrologyClosureRead::Failed;
		}
		RegionMapCapacity = Capacity;
	}
	OutRegion = Regions.Add({InCoordinate, OutGraph->InputFingerprint, OutGraph->Fingerprint, OutGraph});
	ByRegion.Add(InCoordinate, OutRegion);
	Result.InputFingerprint ^= VoxelGeneration::Mix(OutGraph->Fingerprint ^ OutGraph->InputFingerprint ^
		VoxelGeneration::Mix((uint64(uint32(InCoordinate.X)) << 32) | uint32(InCoordinate.Y)));
	return EVoxelHydrologyClosureRead::Ready;
}

EVoxelHydrologyClosureRead FVoxelHydrologyNetworkSearch::ReadKnownRegion(const int32 InRegion, FReadRegion InRead,
	const FVoxelHydrologyRegionGraph*& OutGraph, FString& OutError)
{
	if (const auto* Graph = Regions[InRegion].SliceGraph)
	{
		++RegionReadRequests;
		++SliceGraphHits;
		OutGraph = Graph;
		OutError.Reset();
		return EVoxelHydrologyClosureRead::Ready;
	}
	const FIntPoint Coordinate = Regions[InRegion].Coordinate;
	int32 Found = INDEX_NONE;
	return ReadRegion(Coordinate, InRead, Found, OutGraph, OutError);
}

bool FVoxelHydrologyNetworkSearch::AppendNode(const int32 InRegion, const int32 InLocalIndex,
	const FVoxelHydrologyRegionGraph& InGraph, int32& OutIndex, FString& OutError)
{
	if (!InGraph.Nodes.IsValidIndex(InLocalIndex))
	{
		OutError = TEXT("Hydrology network connection has no retained regional node");
		return false;
	}
	const auto& Source = InGraph.Nodes[InLocalIndex];
	if (const int32* Existing = ByCell.Find(Source.Cell))
	{
		OutIndex = *Existing;
		return true;
	}
	if (!Nodes.ReserveOne(OutError))
	{
		return false;
	}
	if (ByCell.Num() == CellMapCapacity)
	{
		const int32 Capacity = HydrologyNetworkCapacity(CellMapCapacity);
		if (!VoxelGenerationAllocation::RetainMap(ByCell, Capacity, OutError))
		{
			return false;
		}
		CellMapCapacity = Capacity;
	}
	FNode Node;
	Node.Cell = Source.Cell;
	Node.Region = InRegion;
	Node.LocalIndex = InLocalIndex;
	Node.GroundZ = Source.GroundZ;
	Node.WaterZ = Source.WaterZ;
	OutIndex = Nodes.Add(Node);
	ByCell.Add(Source.Cell, OutIndex);
	return true;
}

EVoxelHydrologyClosureRead FVoxelHydrologyNetworkSearch::ReadConnection(const int32 InNode, const int32 InSlot,
	FReadRegion InRead, FConnection& OutConnection, FString& OutError)
{
	OutConnection = {};
	const FNode Node = Nodes[InNode];
	const FVoxelHydrologyRegionGraph* Graph = nullptr;
	const auto Read = ReadKnownRegion(Node.Region, InRead, Graph, OutError);
	if (Read != EVoxelHydrologyClosureRead::Ready)
	{
		return Read;
	}
	const auto& Source = Graph->Nodes[Node.LocalIndex];
	if (InSlot >= Source.EdgeCount + 4)
	{
		OutConnection.bAtEnd = true;
		return EVoxelHydrologyClosureRead::Ready;
	}
	if (InSlot < Source.EdgeCount)
	{
		OutConnection.Edge = Source.Edges[InSlot];
		const auto& Edge = Graph->Edges[OutConnection.Edge];
		OutConnection.bFromFirst = Edge.First == Node.LocalIndex;
		const int32 Other = OutConnection.bFromFirst ? Edge.Second : Edge.First;
		OutConnection.FirstStep = OutConnection.bFromFirst ? Edge.FirstStep : Edge.LastStep;
		OutConnection.BarrierZ = Edge.BarrierZ;
		OutConnection.Steps = Edge.CellSteps;
		OutConnection.LastBarrierDistance = Graph->GetLastBarrierDistance(OutConnection.Edge, OutConnection.bFromFirst);
		if (const int32* Existing = ByCell.Find(Graph->Nodes[Other].Cell))
		{
			OutConnection.Neighbor = *Existing;
		}
		return EVoxelHydrologyClosureRead::Ready;
	}
	const int32 Face = InSlot - Source.EdgeCount;
	if (!(Source.BoundaryFaces & (1 << Face)))
	{
		return EVoxelHydrologyClosureRead::Ready;
	}
	static const FIntPoint Offsets[] = {FIntPoint(-1, 0), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(0, 1)};
	const int64 X = int64(Source.Cell.X) + Offsets[Face].X;
	const int64 Y = int64(Source.Cell.Y) + Offsets[Face].Y;
	if (X < Minimum.X || Y < Minimum.Y || X > Maximum.X || Y > Maximum.Y)
	{
		return EVoxelHydrologyClosureRead::Ready;
	}
	const FIntPoint OtherCell{int32(X), int32(Y)};
	if (const int32* Existing = ByCell.Find(OtherCell))
	{
		OutConnection.Neighbor = *Existing;
	}
	if (OutConnection.Neighbor != INDEX_NONE)
	{
		const auto& Other = Nodes[OutConnection.Neighbor];
		OutConnection.BarrierZ = FMath::Max(Node.IsReceiver() ? Node.WaterZ : Node.GroundZ, Other.IsReceiver() ? Other.WaterZ : Other.GroundZ);
		OutConnection.Steps = 1;
		OutConnection.FirstStep = OtherCell;
		OutConnection.LastBarrierDistance = (Other.IsReceiver() ? Other.WaterZ : Other.GroundZ) == OutConnection.BarrierZ ? 1 : 0;
	}
	return EVoxelHydrologyClosureRead::Ready;
}

EVoxelHydrologyClosureRead FVoxelHydrologyNetworkSearch::ResolveNeighborPotential(const int32 InNode, const FConnection& InConnection,
	FReadRegion InRead, FVoxelHydrologyRegionPotential& OutPotential, FString& OutError)
{
	const auto& Node = Nodes[InNode];
	const auto& Other = Nodes[InConnection.Neighbor];
	if (InConnection.Steps == 1)
	{
		OutPotential = Other.Potential;
		return EVoxelHydrologyClosureRead::Ready;
	}
	const FVoxelHydrologyRegionGraph* Graph = nullptr;
	const auto Read = ReadKnownRegion(Node.Region, InRead, Graph, OutError);
	if (Read != EVoxelHydrologyClosureRead::Ready)
	{
		return Read;
	}
	const auto& Edge = Graph->Edges[InConnection.Edge];
	FVoxelHydrologyRegionRoute Route;
	if (!Graph->ResolveProfile(InConnection.Edge, InConnection.bFromFirst ? 0 : Edge.ProfileCount - 1,
		InConnection.bFromFirst ? Node.Potential : Other.Potential,
		InConnection.bFromFirst ? Other.Potential : Node.Potential, Route, OutError))
	{
		return EVoxelHydrologyClosureRead::Failed;
	}
	OutPotential = Route.Potential;
	return EVoxelHydrologyClosureRead::Ready;
}

bool FVoxelHydrologyNetworkSearch::IsSolveRoot(const FNode& InNode) const
{
	return InNode.IsReceiver() || (bSolvingLower && InNode.UnknownFaces != 0);
}

bool FVoxelHydrologyNetworkSearch::EventLess(const FEvent& InFirst, const FEvent& InSecond) const
{
	if (InFirst.Plane != InSecond.Plane)
	{
		return InFirst.Plane < InSecond.Plane;
	}
	if (InFirst.Distance != InSecond.Distance)
	{
		return InFirst.Distance < InSecond.Distance;
	}
	return HydrologyNetworkCellLess(Nodes[InFirst.Index].Cell, Nodes[InSecond.Index].Cell);
}

bool FVoxelHydrologyNetworkSearch::Push(const FEvent& InEvent, FString& OutError)
{
	if (Frontier.Num() == MAX_int32)
	{
		OutError = TEXT("Oversized: hydrology network frontier");
		return false;
	}
	if (Frontier.Num() == Frontier.Max() && !VoxelGenerationAllocation::RetainArray(Frontier, HydrologyNetworkCapacity(Frontier.Num()), OutError))
	{
		return false;
	}
	int32 Index = Frontier.Add(InEvent);
	while (Index > 0)
	{
		const int32 Parent = (Index - 1) / 2;
		if (!EventLess(Frontier[Index], Frontier[Parent]))
		{
			break;
		}
		Swap(Frontier[Index], Frontier[Parent]);
		Index = Parent;
	}
	return true;
}

FVoxelHydrologyNetworkSearch::FEvent FVoxelHydrologyNetworkSearch::Pop()
{
	check(!Frontier.IsEmpty());
	const FEvent Event = Frontier[0];
	Frontier[0] = Frontier.Last();
	Frontier.RemoveAt(Frontier.Num() - 1, 1, EAllowShrinking::No);
	int32 Index = 0;
	while (int64(Index) * 2 + 1 < Frontier.Num())
	{
		int32 Child = Index * 2 + 1;
		if (Child + 1 < Frontier.Num() && EventLess(Frontier[Child + 1], Frontier[Child]))
		{
			++Child;
		}
		if (!EventLess(Frontier[Child], Frontier[Index]))
		{
			break;
		}
		Swap(Frontier[Index], Frontier[Child]);
		Index = Child;
	}
	return Event;
}

EVoxelHydrologyNetworkStep FVoxelHydrologyNetworkSearch::Failure(FString& OutError) const
{
	if (OutError == TEXT("Canceled"))
	{
		return EVoxelHydrologyNetworkStep::Canceled;
	}
	if (OutError.StartsWith(TEXT("CapacityBlocked")))
	{
		return EVoxelHydrologyNetworkStep::CapacityBlocked;
	}
	if (OutError.IsEmpty())
	{
		OutError = TEXT("Invalid hydrology network state or input");
	}
	return EVoxelHydrologyNetworkStep::Failed;
}

EVoxelHydrologyNetworkStep FVoxelHydrologyNetworkSearch::ReadFailure(const EVoxelHydrologyClosureRead InRead, FString& OutError) const
{
	return InRead == EVoxelHydrologyClosureRead::Pending ? EVoxelHydrologyNetworkStep::NeedInput : Failure(OutError);
}

EVoxelHydrologyNetworkStep FVoxelHydrologyNetworkSearch::Advance(const uint64 InGeneration, FReadRegion InReadRegion,
	int32 InWorkBudget, FString& OutError, const TAtomic<bool>* InCancel, const double InDeadlineSeconds)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_HydrologyControlSearch);
	// 原读取者只保证本次 Advance 的冻结图有效；所有返回路径都归还借用指针。
	ON_SCOPE_EXIT
	{
		for (FRegion& Region : Regions) Region.SliceGraph = nullptr;
	};
	// 续跑可能更换 Worker；跨线程共享的节流仅限制日志，不参与求解状态。
	static TAtomic<uint64> NextDiagnosticMilliseconds{0};
	const uint64 Now = uint64(FPlatformTime::Seconds() * 1000.0);
	uint64 Expected = NextDiagnosticMilliseconds.Load();
	if (Now >= Expected && NextDiagnosticMilliseconds.CompareExchange(Expected, Now + 5000))
	{
		UE_LOG(LogTemp, Display, TEXT("Voxel macro control progress: generation=%llu seed=(%d,%d) stage=%d lower=%d nodes=%d regions=%d cursor=%d frontier=%d directionExpansions=%d receiverExpansions=%d regionReadRequests=%llu sliceGraphHits=%llu"),
			Result.Generation, Seed.X, Seed.Y, int32(Stage), bSolvingLower ? 1 : 0, Nodes.Num(), Regions.Num(), Cursor, Frontier.Num(), DirectionProofExpansions, ReceiverProofExpansions, RegionReadRequests, SliceGraphHits);
	}
	OutError.Reset();
	if (Stage == EStage::None || Stage == EStage::Taken || InGeneration != Result.Generation)
	{
		OutError = TEXT("Invalid hydrology network generation or lifetime");
		return Failure(OutError);
	}
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return Failure(OutError);
	}
	int32 DeadlineCheck = 0;
	while (InWorkBudget-- > 0)
	{
		if (InDeadlineSeconds > 0.0 && (DeadlineCheck++ & 31) == 0 && FPlatformTime::Seconds() >= InDeadlineSeconds) break;
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return Failure(OutError);
		}
		if (Stage == EStage::LoadRegion)
		{
			int32 Region = INDEX_NONE;
			const FVoxelHydrologyRegionGraph* Graph = nullptr;
			const auto Read = ReadRegion(PendingRegion, InReadRegion, Region, Graph, OutError);
			if (Read != EVoxelHydrologyClosureRead::Ready) return ReadFailure(Read, OutError);
			if (LoadCursor < Graph->Nodes.Num())
			{
				int32 Index = INDEX_NONE;
				if (!AppendNode(Region, LoadCursor, *Graph, Index, OutError)) return Failure(OutError);
				++LoadCursor;
				continue;
			}
			const int32* Target = ByCell.Find(Seed);
			if (!Target)
			{
				OutError = TEXT("Hydrology network seed has no retained regional node");
				return Failure(OutError);
			}
			TargetIndex = *Target;
			Cursor = 0;
			Frontier.Reset();
			bSolvingLower = true;
			Stage = EStage::ResetSolve;
		}
		else if (Stage == EStage::ResetSolve)
		{
			if (Cursor == Nodes.Num())
			{
				Cursor = 0;
				Stage = EStage::SpillSeeds;
				continue;
			}
			auto& Node = Nodes[Cursor];
			const FVoxelHydrologyRegionGraph* Graph = nullptr;
			const auto Read = ReadKnownRegion(Node.Region, InReadRegion, Graph, OutError);
			if (Read != EVoxelHydrologyClosureRead::Ready) return ReadFailure(Read, OutError);
			Node.UnknownFaces = 0;
			static const FIntPoint Offsets[] = {FIntPoint(-1, 0), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(0, 1)};
			for (int32 Face = 0; Face < 4; ++Face)
			{
				if (!(Graph->Nodes[Node.LocalIndex].BoundaryFaces & (1 << Face))) continue;
				const int64 X = int64(Node.Cell.X) + Offsets[Face].X;
				const int64 Y = int64(Node.Cell.Y) + Offsets[Face].Y;
				if (X >= Minimum.X && Y >= Minimum.Y && X <= Maximum.X && Y <= Maximum.Y &&
					!ByCell.Contains(FIntPoint(int32(X), int32(Y)))) Node.UnknownFaces |= 1 << Face;
			}
			Node.Potential = {};
			Node.ParentCell = FIntPoint::ZeroValue;
			Node.bHasParent = false;
			Node.bIncluded = false;
			Node.ParentIndex = INDEX_NONE;
			Node.ReceiverFloorZ = MIN_int32;
			Node.ReceiverBlocker = INDEX_NONE;
			Node.ReceiverCaller = INDEX_NONE;
			Node.ReceiverState = 0;
			++Cursor;
		}
		else if (Stage == EStage::SpillSeeds)
		{
			if (Cursor == Nodes.Num())
			{
				Current = INDEX_NONE;
				Stage = EStage::Spill;
				continue;
			}
			FNode& Node = Nodes[Cursor];
			if (IsSolveRoot(Node))
			{
				if (!Push({Cursor, Node.IsReceiver() ? Node.WaterZ : Node.GroundZ, 0}, OutError))
				{
					return Failure(OutError);
				}
				Node.Potential.SpillZ = Node.IsReceiver() ? Node.WaterZ : Node.GroundZ;
			}
			++Cursor;
		}
		else if (Stage == EStage::Spill || Stage == EStage::Plateaus)
		{
			const bool bPlateaus = Stage == EStage::Plateaus;
			if (Current == INDEX_NONE)
			{
				if (Frontier.IsEmpty())
				{
					Cursor = 0;
					Slot = 0;
					Stage = bPlateaus ? EStage::Parents : EStage::Anchors;
					continue;
				}
				ActiveEvent = Pop();
				const auto& Potential = Nodes[ActiveEvent.Index].Potential;
				if (ActiveEvent.Plane != Potential.SpillZ || (bPlateaus && ActiveEvent.Distance != Potential.PlateauDistance))
				{
					continue;
				}
				Current = ActiveEvent.Index;
				Slot = 0;
			}
			FConnection Connection;
			const auto Read = ReadConnection(Current, Slot, InReadRegion, Connection, OutError);
			if (Read != EVoxelHydrologyClosureRead::Ready)
			{
				return ReadFailure(Read, OutError);
			}
			if (Connection.bAtEnd)
			{
				Current = INDEX_NONE;
				continue;
			}
			if (Connection.Neighbor != INDEX_NONE)
			{
				FNode& Other = Nodes[Connection.Neighbor];
				if (!IsSolveRoot(Other))
				{
					if (!bPlateaus)
					{
						const int32 Plane = FMath::Max(ActiveEvent.Plane, Connection.BarrierZ);
						if (Plane < Other.Potential.SpillZ)
						{
							if (!Push({Connection.Neighbor, Plane, 0}, OutError))
							{
								return Failure(OutError);
							}
							Other.Potential.SpillZ = Plane;
						}
					}
					else if (Other.Potential.SpillZ == ActiveEvent.Plane && Connection.BarrierZ <= ActiveEvent.Plane)
					{
						if (ActiveEvent.Distance > MAX_int64 - Connection.Steps)
						{
							OutError = TEXT("Oversized: hydrology network plateau distance");
							return Failure(OutError);
						}
						const int64 Distance = ActiveEvent.Distance + Connection.Steps;
						if (Distance < Other.Potential.PlateauDistance)
						{
							if (!Push({Connection.Neighbor, ActiveEvent.Plane, Distance}, OutError))
							{
								return Failure(OutError);
							}
							Other.Potential.PlateauDistance = Distance;
						}
					}
				}
			}
			++Slot;
		}
		else if (Stage == EStage::Anchors || Stage == EStage::Parents)
		{
			const bool bParents = Stage == EStage::Parents;
			if (Cursor == Nodes.Num())
			{
				Current = INDEX_NONE;
				Stage = bParents ? (bSolvingLower ? EStage::SaveLower : EStage::Certify) : EStage::Plateaus;
				Cursor = 0;
				continue;
			}
			FNode& Node = Nodes[Cursor];
			if (Node.Potential.SpillZ == MAX_int32)
			{
				++Cursor;
				Slot = 0;
				continue;
			}
			if (bParents && Node.Potential.PlateauDistance == MAX_int64)
			{
				OutError = TEXT("Hydrology network contains an unresolved bounded plateau");
				return Failure(OutError);
			}
			if (IsSolveRoot(Node))
			{
				if (!bParents)
				{
					if (!Push({Cursor, Node.Potential.SpillZ, 0}, OutError)) return Failure(OutError);
					Node.Potential.PlateauDistance = 0;
				}
				++Cursor;
				Slot = 0;
				continue;
			}
			FConnection Connection;
			const auto Read = ReadConnection(Cursor, Slot, InReadRegion, Connection, OutError);
			if (Read != EVoxelHydrologyClosureRead::Ready)
			{
				return ReadFailure(Read, OutError);
			}
			if (Connection.bAtEnd)
			{
				if (bParents && !Node.bHasParent)
				{
					OutError = TEXT("Hydrology network dry node has no descending parent");
					return Failure(OutError);
				}
				if (!bParents && Node.Potential.PlateauDistance != MAX_int64 &&
					!Push({Cursor, Node.Potential.SpillZ, Node.Potential.PlateauDistance}, OutError))
				{
					return Failure(OutError);
				}
				++Cursor;
				Slot = 0;
				continue;
			}
			if (Connection.Neighbor != INDEX_NONE && Connection.BarrierZ <= Node.Potential.SpillZ)
			{
				const FNode& Other = Nodes[Connection.Neighbor];
				if (!bParents && Other.Potential.SpillZ < Node.Potential.SpillZ)
				{
					Node.Potential.PlateauDistance = FMath::Min<int64>(Node.Potential.PlateauDistance, Connection.LastBarrierDistance);
				}
				else if (bParents)
				{
					FVoxelHydrologyRegionPotential NeighborPotential;
					const auto NeighborRead = ResolveNeighborPotential(Cursor, Connection, InReadRegion, NeighborPotential, OutError);
					if (NeighborRead != EVoxelHydrologyClosureRead::Ready)
					{
						return ReadFailure(NeighborRead, OutError);
					}
					const bool bDescending = NeighborPotential.SpillZ < Node.Potential.SpillZ;
					const bool bPlateauParent = NeighborPotential.SpillZ == Node.Potential.SpillZ &&
						NeighborPotential.PlateauDistance < MAX_int64 && NeighborPotential.PlateauDistance + 1 == Node.Potential.PlateauDistance;
					if ((bDescending || bPlateauParent) && (!Node.bHasParent || HydrologyNetworkCellLess(Connection.FirstStep, Node.ParentCell)))
					{
						Node.ParentCell = Connection.FirstStep;
						Node.ParentIndex = Connection.Neighbor;
						Node.bHasParent = true;
					}
				}
			}
			++Slot;
		}
		else if (Stage == EStage::SaveLower)
		{
			if (Cursor == Nodes.Num())
			{
				Cursor = 0;
				Frontier.Reset();
				bSolvingLower = false;
				Stage = EStage::ResetSolve;
				continue;
			}
			auto& Node = Nodes[Cursor++];
			Node.LowerPotential = Node.Potential;
			Node.LowerParentCell = Node.ParentCell;
			Node.bLowerHasParent = Node.bHasParent;
		}
		else if (Stage == EStage::Certify)
		{
			if (Cursor == 0)
			{
				IncludedCount = 0;
				if (bEntireRegion) TargetIndex = INDEX_NONE;
			}
			if (Cursor < Nodes.Num())
			{
				auto& Node = Nodes[Cursor++];
				// 未知边界在 lower 中仅作最乐观的地面锚点；upper 只来自真实湿根。
				// 势值及最小父方向两端一致时，任意合法境外延伸都不能改变该控制。
				Node.bIncluded = Node.Potential.SpillZ != MAX_int32 && Node.Potential == Node.LowerPotential &&
					Node.bHasParent == Node.bLowerHasParent && (!Node.bHasParent || Node.ParentCell == Node.LowerParentCell);
				IncludedCount += Node.bIncluded;
				if (bEntireRegion && Node.Region == 0 && !Node.bIncluded && TargetIndex == INDEX_NONE) TargetIndex = Cursor - 1;
				continue;
			}
			if (bEntireRegion ? TargetIndex == INDEX_NONE : Nodes[TargetIndex].bIncluded)
			{
				if (!VoxelGenerationAllocation::RetainArray(Result.Cells, IncludedCount, OutError)) return Failure(OutError);
				Result.Threshold = Nodes[ByCell.FindChecked(Seed)].Potential.SpillZ;
				Cursor = 0;
				Current = INDEX_NONE;
				Stage = EStage::ReceiverProof;
				continue;
			}
			if (Nodes[TargetIndex].LowerPotential.SpillZ == MAX_int32)
			{
				OutError = TEXT("Hydrology network component has no physical wet receiver");
				return EVoxelHydrologyNetworkStep::NoReceiver;
			}
			++DirectionProofExpansions;
			Current = TargetIndex;
			Slot = 0;
			TraceSteps = 0;
			Stage = EStage::TraceBoundary;
		}
		else if (Stage == EStage::TraceBoundary)
		{
			const auto& Node = Nodes[Current];
			if (!Node.bLowerHasParent)
			{
				if (!Node.IsReceiver() && Node.UnknownFaces)
				{
					static const FIntPoint Offsets[] = {FIntPoint(-1, 0), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(0, 1)};
					for (int32 Face = 0; Face < 4; ++Face)
					{
						if (!(Node.UnknownFaces & (1 << Face))) continue;
						const FIntPoint Other = Node.Cell + Offsets[Face];
						PendingRegion = FIntPoint(VoxelGeneration::FloorDivide(Other.X, RegionSide), VoxelGeneration::FloorDivide(Other.Y, RegionSide));
						LoadCursor = 0;
						Stage = EStage::LoadRegion;
						break;
					}
					continue;
				}
				OutError = TEXT("Hydrology network unresolved proof has no optimistic boundary dependency");
				return Failure(OutError);
			}
			FConnection Connection;
			const auto Read = ReadConnection(Current, Slot, InReadRegion, Connection, OutError);
			if (Read != EVoxelHydrologyClosureRead::Ready) return ReadFailure(Read, OutError);
			if (Connection.bAtEnd || TraceSteps > Nodes.Num())
			{
				OutError = TEXT("Hydrology network bounded proof dependency is invalid or cyclic");
				return Failure(OutError);
			}
			if (Connection.Neighbor != INDEX_NONE && Connection.FirstStep == Node.LowerParentCell)
			{
				Current = Connection.Neighbor;
				Slot = 0;
				++TraceSteps;
			}
			else ++Slot;
		}
		else if (Stage == EStage::ReceiverProof)
		{
			// Follow only certified parents in the existing proof graph. An uncertified
			// downstream node must expand the proof, never become an invented outlet.
			if (Current == INDEX_NONE)
			{
				if (Cursor == Nodes.Num())
				{
					Cursor = 0;
					Stage = EStage::ReceiverValidate;
					continue;
				}
				Current = Cursor++;
			}
			auto& Node = Nodes[Current];
			if (Node.ReceiverState == 0)
			{
				if (!Node.bIncluded)
				{
					Node.ReceiverBlocker = Current;
					Node.ReceiverState = 2;
				}
				else if (Node.IsReceiver())
				{
					Node.ReceiverFloorZ = Node.WaterZ;
					Node.ReceiverState = 2;
				}
				else
				{
					if (!Node.bHasParent || Node.ParentIndex == INDEX_NONE || Nodes[Node.ParentIndex].ReceiverState == 1)
					{
						OutError = TEXT("Hydrology network receiver proof has an invalid or cyclic parent");
						return Failure(OutError);
					}
					Node.ReceiverState = 1;
					if (Nodes[Node.ParentIndex].ReceiverState == 0)
					{
						Nodes[Node.ParentIndex].ReceiverCaller = Current;
						Current = Node.ParentIndex;
						continue;
					}
				}
			}
			if (Node.ReceiverState == 1)
			{
				const auto& Parent = Nodes[Node.ParentIndex];
				check(Parent.ReceiverState == 2);
				Node.ReceiverFloorZ = Parent.ReceiverFloorZ;
				Node.ReceiverBlocker = Parent.ReceiverBlocker;
				Node.ReceiverState = 2;
			}
			Current = Node.ReceiverCaller;
			Node.ReceiverCaller = INDEX_NONE;
		}
		else if (Stage == EStage::ReceiverValidate)
		{
			if (Cursor == Nodes.Num())
			{
				Cursor = 0;
				Stage = EStage::Emit;
				continue;
			}
			const int32 Index = Cursor++;
			const auto& Node = Nodes[Index];
			if ((bEntireRegion ? Node.Region == 0 : Node.Cell == Seed) && Node.ReceiverFloorZ == MIN_int32)
			{
				if (Node.ReceiverBlocker == INDEX_NONE)
				{
					OutError = TEXT("Hydrology network receiver proof has no unresolved dependency");
					return Failure(OutError);
				}
				++ReceiverProofExpansions;
				TargetIndex = Node.ReceiverBlocker;
				Current = TargetIndex;
				Slot = 0;
				TraceSteps = 0;
				Stage = EStage::TraceBoundary;
			}
		}
		else if (Stage == EStage::Emit)
		{
			if (Cursor == Nodes.Num())
			{
				SortRoot = Result.Cells.Num() / 2 - 1;
				SortEnd = Result.Cells.Num() - 1;
				Stage = EStage::Sort;
				continue;
			}
			const auto& Node = Nodes[Cursor++];
			if (Node.bIncluded && Node.ReceiverFloorZ != MIN_int32)
			{
				Result.Cells.Add({Node.Cell, Node.Potential, Node.ParentCell, Node.bHasParent, Node.ReceiverFloorZ});
			}
		}
		else if (Stage == EStage::Sort)
		{
			if (SortRoot >= 0)
			{
				SiftResult(SortRoot--, SortEnd);
			}
			else if (SortEnd > 0)
			{
				Swap(Result.Cells[0], Result.Cells[SortEnd]);
				SiftResult(0, --SortEnd);
			}
			else
			{
				Result.Fingerprint = VoxelGeneration::Mix(Result.Generation ^ Result.InputFingerprint ^ uint64(uint32(Result.Threshold)));
				Cursor = 0;
				Stage = EStage::Hash;
			}
		}
		else if (Stage == EStage::Hash)
		{
			if (Cursor < Result.Cells.Num())
			{
				const auto& Cell = Result.Cells[Cursor++];
				Result.Fingerprint = VoxelGeneration::Mix(Result.Fingerprint ^ ((uint64(uint32(Cell.Cell.X)) << 32) | uint32(Cell.Cell.Y)));
				Result.Fingerprint = VoxelGeneration::Mix(Result.Fingerprint ^ uint64(uint32(Cell.Potential.SpillZ)) ^ uint64(Cell.Potential.PlateauDistance));
				Result.Fingerprint = VoxelGeneration::Mix(Result.Fingerprint ^ uint32(Cell.ReceiverFloorZ));
				Result.Fingerprint = VoxelGeneration::Mix(Result.Fingerprint ^ ((uint64(uint32(Cell.ParentCell.X)) << 32) | uint32(Cell.ParentCell.Y)) ^ uint64(Cell.bHasParent));
				continue;
			}
			if (!Result.Fingerprint)
			{
				Result.Fingerprint = 1;
			}
			FreeScratch();
			Stage = EStage::Ready;
		}
		else if (Stage == EStage::Ready)
		{
			return EVoxelHydrologyNetworkStep::Ready;
		}
	}
	return Stage == EStage::Ready ? EVoxelHydrologyNetworkStep::Ready : EVoxelHydrologyNetworkStep::Yielded;
}

void FVoxelHydrologyNetworkSearch::SiftResult(int32 InRoot, const int32 InEnd)
{
	while (int64(InRoot) * 2 + 1 <= InEnd)
	{
		int32 Child = InRoot * 2 + 1;
		if (Child < InEnd && HydrologyNetworkCellLess(Result.Cells[Child].Cell, Result.Cells[Child + 1].Cell))
		{
			++Child;
		}
		if (!HydrologyNetworkCellLess(Result.Cells[InRoot].Cell, Result.Cells[Child].Cell))
		{
			break;
		}
		Swap(Result.Cells[InRoot], Result.Cells[Child]);
		InRoot = Child;
	}
}

void FVoxelHydrologyNetworkSearch::FreeScratch()
{
	Nodes.FreeBacking();
	VoxelGenerationAllocation::FreeRetainedArray(Regions);
	VoxelGenerationAllocation::FreeRetainedMap(ByCell);
	VoxelGenerationAllocation::FreeRetainedMap(ByRegion);
	VoxelGenerationAllocation::FreeRetainedArray(Frontier);
	CellMapCapacity = 0;
	RegionMapCapacity = 0;
}

const FVoxelHydrologyNetworkResult& FVoxelHydrologyNetworkSearch::GetResult() const
{
	check(Stage == EStage::Ready);
	return Result;
}

FVoxelHydrologyNetworkResult FVoxelHydrologyNetworkSearch::TakeResult()
{
	check(Stage == EStage::Ready);
	Stage = EStage::Taken;
	return MoveTemp(Result);
}

uint64 FVoxelHydrologyNetworkSearch::GetAllocatedBytes() const
{
	return Result.GetAllocatedBytes() + Nodes.GetAllocatedBytes() + VoxelGenerationAllocation::AllocatedBytes(Regions) +
		VoxelGenerationAllocation::AllocatedBytes(ByCell) + VoxelGenerationAllocation::AllocatedBytes(ByRegion) +
		VoxelGenerationAllocation::AllocatedBytes(Frontier);
}

int32 FVoxelHydrologyNetworkSearch::GetNodeCount() const
{
	return Nodes.Num();
}

int32 FVoxelHydrologyNetworkSearch::GetRegionCount() const
{
	return Regions.Num();
}
