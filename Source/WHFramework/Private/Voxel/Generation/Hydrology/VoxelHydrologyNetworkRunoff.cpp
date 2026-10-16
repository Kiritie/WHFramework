#include "Voxel/Generation/Hydrology/VoxelHydrologyNetworkRunoff.h"

#include "Voxel/Generation/VoxelGenerationMath.h"

namespace
{
	uint64 HydrologyRunoffControlFingerprint(const FVoxelHydrologyNetworkCell& InControl)
	{
		uint64 Fingerprint = VoxelGeneration::Mix((uint64(uint32(InControl.Cell.X)) << 32) | uint32(InControl.Cell.Y));
		Fingerprint = VoxelGeneration::Mix(Fingerprint ^ uint64(uint32(InControl.Potential.SpillZ)) ^ uint64(InControl.Potential.PlateauDistance));
		return VoxelGeneration::Mix(Fingerprint ^ ((uint64(uint32(InControl.ParentCell.X)) << 32) |
			uint32(InControl.ParentCell.Y)) ^ uint64(InControl.bHasParent));
	}

	int32 HydrologyRunoffCapacity(const int32 InCapacity)
	{
		return int32(FMath::Min<int64>(MAX_int32, FMath::Max<int64>(16, int64(InCapacity) * 2)));
	}
}

const FVoxelHydrologyNetworkRunoffCell* FVoxelHydrologyNetworkRunoff::Find(const FIntPoint& InCell) const
{
	int32 Lower = 0;
	int32 Upper = Cells.Num();
	while (Lower < Upper)
	{
		const int32 Middle = Lower + (Upper - Lower) / 2;
		const FIntPoint Cell = Cells[Middle].Control.Cell;
		if (Cell.Y < InCell.Y || (Cell.Y == InCell.Y && Cell.X < InCell.X)) Lower = Middle + 1;
		else Upper = Middle;
	}
	return Cells.IsValidIndex(Lower) && Cells[Lower].Control.Cell == InCell ? &Cells[Lower] : nullptr;
}

uint64 FVoxelHydrologyNetworkRunoff::GetAllocatedBytes() const
{
	return VoxelGenerationAllocation::AllocatedBytes(Cells);
}

bool FVoxelHydrologySourceCell::IsQualified(const uint64 InThreshold) const
{
	const uint64 Threshold = FMath::Max<uint64>(1, InThreshold);
	return !bReceiver && bHasParent && Accumulation >= Threshold && MaxChildRainWeight < Threshold;
}

bool FVoxelHydrologyNetworkRunoff::SampleSourceCell(const FVoxelHydrologyRegionGraph& InGraph,
	const FIntPoint& InCell, FVoxelHydrologySourceCell& OutCell, FString& OutError) const
{
	if (!Fingerprint || Generation != InGraph.Generation || GraphFingerprint != InGraph.Fingerprint)
	{
		OutError = TEXT("Hydrology source query requires matching completed macro runoff");
		return false;
	}
	int32 Location = INDEX_NONE;
	const auto Kind = InGraph.FindCellLocation(InCell, Location);
	FVoxelHydrologySourceCell Source;
	Source.Cell = InCell;
	if (Kind == EVoxelHydrologyRegionCellKind::Wet)
	{
		Source.bReceiver = true;
	}
	else if (Kind == EVoxelHydrologyRegionCellKind::Node)
	{
		const auto& Facts = InGraph.Nodes[Location];
		const auto* Flow = Find(Facts.Cell);
		if (!Flow || Facts.Cell != InCell)
		{
			OutError = TEXT("Hydrology source query has no resolved critical node");
			return false;
		}
		Source.Accumulation = Flow->Accumulation;
		Source.MaxChildRainWeight = Flow->MaxChildRainWeight;
		Source.ParentCell = Flow->Control.ParentCell;
		Source.bHasParent = Flow->Control.bHasParent;
		Source.bReceiver = Facts.IsReceiver();
	}
	else if (Kind == EVoxelHydrologyRegionCellKind::Pruned)
	{
		const auto& Branch = InGraph.PrunedCells[Location];
		if (InGraph.Minimum + FIntPoint(Branch.CellIndex % InGraph.Width, Branch.CellIndex / InGraph.Width) != InCell)
		{
			OutError = TEXT("Hydrology source query has inconsistent pruned location");
			return false;
		}
		Source.Accumulation = Branch.RainWeight;
		Source.MaxChildRainWeight = InGraph.GetMaxPrunedChildRainWeight(Branch.CellIndex);
		Source.ParentCell = InGraph.Minimum + FIntPoint(Branch.Parent % InGraph.Width, Branch.Parent / InGraph.Width);
		Source.bHasParent = true;
	}
	else if (Kind == EVoxelHydrologyRegionCellKind::Profile)
	{
		const int32 EdgeIndex = InGraph.FindProfileEdge(Location);
		if (EdgeIndex == INDEX_NONE || InGraph.GetProfileCell(Location) != InCell)
		{
			OutError = TEXT("Hydrology source query has inconsistent edge profile");
			return false;
		}
		const auto& Edge = InGraph.Edges[EdgeIndex];
		const auto* First = Find(InGraph.Nodes[Edge.First].Cell);
		const auto* Second = Find(InGraph.Nodes[Edge.Second].Cell);
		if (!First || !Second)
		{
			OutError = TEXT("Hydrology source query requires both resolved edge endpoints");
			return false;
		}
		const int32 Offset = Location - Edge.ProfileOffset;
		FVoxelHydrologyRegionRoute Route;
		FVoxelHydrologyRegionSplit Split;
		if (!InGraph.ResolveProfile(EdgeIndex, Offset, First->Control.Potential, Second->Control.Potential, Route, OutError) ||
			!InGraph.SplitEdgeRain(EdgeIndex, First->Control.Potential, Second->Control.Potential, Split, OutError)) return false;
		const auto& Profile = InGraph.Profiles[Location];
		const uint64 PrefixBefore = Offset == 0 ? 0 : InGraph.Profiles[Location - 1].RainPrefix;
		if (Profile.RainPrefix < PrefixBefore || (Route.bTowardFirst ? Split.FirstRainWeight < PrefixBefore : Profile.RainPrefix < Split.FirstRainWeight))
		{
			OutError = TEXT("Hydrology source query has inconsistent split rainfall prefix");
			return false;
		}
		const uint64 OwnRain = Profile.RainPrefix - PrefixBefore;
		const uint64 LocalRain = Route.bTowardFirst ? Split.FirstRainWeight - PrefixBefore : Profile.RainPrefix - Split.FirstRainWeight;
		const auto& Other = Route.bTowardFirst ? *Second : *First;
		const bool bOtherIntoEdge = Other.Control.bHasParent && Other.Control.ParentCell == (Route.bTowardFirst ? Edge.LastStep : Edge.FirstStep);
		const uint64 Incoming = bOtherIntoEdge ? Other.Accumulation : 0;
		if (MAX_uint64 - LocalRain < Incoming || LocalRain < OwnRain)
		{
			OutError = TEXT("Oversized or invalid macro profile source accumulation");
			return false;
		}
		Source.Accumulation = LocalRain + Incoming;
		Source.MaxChildRainWeight = FMath::Max(InGraph.GetMaxPrunedChildRainWeight(Profile.CellIndex), Source.Accumulation - OwnRain);
		Source.ParentCell = Route.ParentCell;
		Source.bHasParent = true;
	}
	else
	{
		OutError = TEXT("Hydrology source query is outside the frozen graph or has no location");
		return false;
	}
	OutCell = Source;
	OutError.Reset();
	return true;
}

FVoxelHydrologyNetworkRunoffBuild::FVoxelHydrologyNetworkRunoffBuild()
{
}

FVoxelHydrologyNetworkRunoffBuild::~FVoxelHydrologyNetworkRunoffBuild()
{
}

bool FVoxelHydrologyNetworkRunoffBuild::Initialize(const FIntPoint& InCoordinate, const int32 InCellSize,
	const int32 InRegionSide, const uint64 InGeneration, FString& OutError)
{
	if (Stage != EStage::None || InCellSize <= 0 || InRegionSide <= 0 || InRegionSide > 4096 || !InGeneration)
	{
		OutError = TEXT("Invalid hydrology network runoff generation or dimensions");
		return false;
	}
	const int64 Lower = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMin, InCellSize);
	const int64 Upper = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMaxExclusive - 1, InCellSize);
	if (int64(InCoordinate.X) * InRegionSide > Upper || int64(InCoordinate.Y) * InRegionSide > Upper ||
		(int64(InCoordinate.X) + 1) * InRegionSide <= Lower || (int64(InCoordinate.Y) + 1) * InRegionSide <= Lower)
	{
		OutError = TEXT("Hydrology runoff region is outside the physical world");
		return false;
	}
	Minimum = FIntPoint(int32(Lower), int32(Lower));
	Maximum = FIntPoint(int32(Upper), int32(Upper));
	Result.Coordinate = InCoordinate;
	Result.Generation = InGeneration;
	CellSize = InCellSize;
	RegionSide = InRegionSide;
	Stage = EStage::Seeds;
	OutError.Reset();
	return true;
}

FIntPoint FVoxelHydrologyNetworkRunoffBuild::RegionFor(const FIntPoint& InCell) const
{
	return FIntPoint(VoxelGeneration::FloorDivide(InCell.X, RegionSide), VoxelGeneration::FloorDivide(InCell.Y, RegionSide));
}

bool FVoxelHydrologyNetworkRunoffBuild::IsPhysical(const FIntPoint& InCell) const
{
	return InCell.X >= Minimum.X && InCell.Y >= Minimum.Y && InCell.X <= Maximum.X && InCell.Y <= Maximum.Y;
}

EVoxelHydrologyClosureRead FVoxelHydrologyNetworkRunoffBuild::ReadGraph(const FIntPoint& InCoordinate,
	FReadRegion InRead, const FVoxelHydrologyRegionGraph*& OutGraph, FString& OutError) const
{
	OutGraph = nullptr;
	const auto Read = InRead(InCoordinate, OutGraph, OutError);
	if (Read != EVoxelHydrologyClosureRead::Ready) return Read;
	if (!OutGraph)
	{
		OutError = TEXT("Hydrology runoff received no published region");
		return EVoxelHydrologyClosureRead::Failed;
	}
	return OutGraph->ValidateCore(InCoordinate, CellSize, RegionSide, Result.Generation, OutError) ?
		EVoxelHydrologyClosureRead::Ready : EVoxelHydrologyClosureRead::Failed;
}

EVoxelHydrologyClosureRead FVoxelHydrologyNetworkRunoffBuild::ReadControl(const FIntPoint& InCell,
	FReadRegion InReadRegion, FReadControl InReadControl, FVoxelHydrologyNetworkCell& OutControl,
	uint64& OutGraphFingerprint, FString& OutError) const
{
	const FVoxelHydrologyRegionGraph* Graph = nullptr;
	const auto GraphRead = ReadGraph(RegionFor(InCell), InReadRegion, Graph, OutError);
	if (GraphRead != EVoxelHydrologyClosureRead::Ready) return GraphRead;
	const int32 LocalIndex = Graph->FindNodeIndex(InCell);
	if (!Graph->Nodes.IsValidIndex(LocalIndex))
	{
		OutError = TEXT("Hydrology runoff control has no retained regional node");
		return EVoxelHydrologyClosureRead::Failed;
	}
	const FVoxelHydrologyRegionNode Facts = Graph->Nodes[LocalIndex];
	const uint64 Fingerprint = Graph->Fingerprint;
	FIntPoint ParentOptions[8];
	int32 ParentCount = 0;
	for (int32 Slot = 0; Slot < Facts.EdgeCount; ++Slot)
	{
		const auto& Edge = Graph->Edges[Facts.Edges[Slot]];
		if (Edge.First == LocalIndex) ParentOptions[ParentCount++] = Edge.FirstStep;
		else ParentOptions[ParentCount++] = Edge.LastStep;
	}
	static const FIntPoint Offsets[] = {FIntPoint(-1, 0), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(0, 1)};
	for (int32 Face = 0; Face < 4; ++Face)
	{
		if (!(Facts.BoundaryFaces & (1 << Face))) continue;
		const int64 X = int64(InCell.X) + Offsets[Face].X;
		const int64 Y = int64(InCell.Y) + Offsets[Face].Y;
		if (X >= Minimum.X && Y >= Minimum.Y && X <= Maximum.X && Y <= Maximum.Y)
		{
			ParentOptions[ParentCount++] = FIntPoint(int32(X), int32(Y));
		}
	}
	FVoxelHydrologyNetworkCell Control;
	const auto Read = InReadControl(InCell, Control, OutError);
	if (Read != EVoxelHydrologyClosureRead::Ready) return Read;
	const int64 Distance = FMath::Abs(int64(Control.ParentCell.X) - InCell.X) + FMath::Abs(int64(Control.ParentCell.Y) - InCell.Y);
	bool bForestParent = false;
	for (int32 Option = 0; Option < ParentCount; ++Option)
	{
		bForestParent |= ParentOptions[Option] == Control.ParentCell;
	}
	if (Control.Cell != InCell || Control.Potential.SpillZ == MAX_int32 || Control.Potential.SpillZ < Facts.GroundZ ||
		Control.Potential.PlateauDistance < 0 || Control.Potential.PlateauDistance == MAX_int64 ||
		(Facts.IsReceiver() ? (Control.bHasParent || Control.Potential.SpillZ != Facts.WaterZ || Control.Potential.PlateauDistance != 0) :
			(!Control.bHasParent || Distance != 1 || !IsPhysical(Control.ParentCell) || !bForestParent)))
	{
		OutError = TEXT("Hydrology runoff requires resolved canonical control and physical parent");
		return EVoxelHydrologyClosureRead::Failed;
	}
	if (const int32* Existing = ByCell.Find(InCell))
	{
		const auto& Node = Nodes[*Existing];
		if (Node.State != ENodeState::None && (Node.Control != Control || Node.GraphFingerprint != Fingerprint))
		{
			OutError = TEXT("Hydrology runoff control changed after admission");
			return EVoxelHydrologyClosureRead::Failed;
		}
	}
	OutControl = Control;
	OutGraphFingerprint = Fingerprint;
	return EVoxelHydrologyClosureRead::Ready;
}

EVoxelHydrologyClosureRead FVoxelHydrologyNetworkRunoffBuild::ReadContribution(const int32 InNode,
	FReadRegion InReadRegion, FReadControl InReadControl, FContribution& OutContribution, FString& OutError) const
{
	OutContribution = {};
	const FNode Node = Nodes[InNode];
	const FVoxelHydrologyRegionGraph* Graph = nullptr;
	const auto Read = ReadGraph(RegionFor(Node.Cell), InReadRegion, Graph, OutError);
	if (Read != EVoxelHydrologyClosureRead::Ready) return Read;
	if (Graph->Fingerprint != Node.GraphFingerprint || !Graph->Nodes.IsValidIndex(Node.LocalIndex) || Graph->Nodes[Node.LocalIndex].Cell != Node.Cell)
	{
		OutError = TEXT("Hydrology runoff frozen source region changed");
		return EVoxelHydrologyClosureRead::Failed;
	}
	const FVoxelHydrologyRegionNode Facts = Graph->Nodes[Node.LocalIndex];
	if (Node.Slot >= Facts.EdgeCount + 4)
	{
		OutContribution.bAtEnd = true;
		return EVoxelHydrologyClosureRead::Ready;
	}
	if (Node.Slot < Facts.EdgeCount)
	{
		const int32 EdgeIndex = Facts.Edges[Node.Slot];
		const FVoxelHydrologyRegionEdge Edge = Graph->Edges[EdgeIndex];
		const bool bFirst = Edge.First == Node.LocalIndex;
		const FIntPoint OtherCell = Graph->Nodes[bFirst ? Edge.Second : Edge.First].Cell;
		if (Edge.First == Edge.Second)
		{
			for (int32 Previous = 0; Previous < Node.Slot; ++Previous)
			{
				if (Facts.Edges[Previous] == EdgeIndex) return EVoxelHydrologyClosureRead::Ready;
			}
			if (Node.Control.bHasParent && (Node.Control.ParentCell == Edge.FirstStep || Node.Control.ParentCell == Edge.LastStep))
			{
				OutError = TEXT("Hydrology runoff parent enters a regional self cycle");
				return EVoxelHydrologyClosureRead::Failed;
			}
			FVoxelHydrologyRegionSplit Split;
			if (!Graph->SplitEdgeRain(EdgeIndex, Node.Control.Potential, Node.Control.Potential, Split, OutError)) return EVoxelHydrologyClosureRead::Failed;
			OutContribution.RainWeight = Edge.InteriorRainWeight;
			OutContribution.MaxChildRainWeight = FMath::Max(Split.FirstRainWeight, Split.SecondRainWeight);
			OutContribution.InputFingerprint = VoxelGeneration::Mix(Node.GraphFingerprint ^ uint64(EdgeIndex));
			return EVoxelHydrologyClosureRead::Ready;
		}
		FVoxelHydrologyNetworkCell Other;
		uint64 OtherFingerprint = 0;
		const auto OtherRead = ReadControl(OtherCell, InReadRegion, InReadControl, Other, OtherFingerprint, OutError);
		if (OtherRead != EVoxelHydrologyClosureRead::Ready) return OtherRead;
		const auto Again = ReadGraph(RegionFor(Node.Cell), InReadRegion, Graph, OutError);
		if (Again != EVoxelHydrologyClosureRead::Ready) return Again;
		if (Graph->Fingerprint != Node.GraphFingerprint || OtherFingerprint != Node.GraphFingerprint)
		{
			OutError = TEXT("Hydrology runoff edge changed during control resolution");
			return EVoxelHydrologyClosureRead::Failed;
		}
		FVoxelHydrologyRegionSplit Split;
		if (!Graph->SplitEdgeRain(EdgeIndex, bFirst ? Node.Control.Potential : Other.Potential,
			bFirst ? Other.Potential : Node.Control.Potential, Split, OutError)) return EVoxelHydrologyClosureRead::Failed;
		const bool bCurrentIntoEdge = Node.Control.bHasParent && Node.Control.ParentCell == (bFirst ? Edge.FirstStep : Edge.LastStep);
		const bool bOtherIntoEdge = Other.bHasParent && Other.ParentCell == (bFirst ? Edge.LastStep : Edge.FirstStep);
		const int32 TowardCurrent = bFirst ? Split.FirstProfileCount : Edge.ProfileCount - Split.FirstProfileCount;
		if ((bCurrentIntoEdge && bOtherIntoEdge) || (bCurrentIntoEdge && TowardCurrent != 0) ||
			(bOtherIntoEdge && TowardCurrent != Edge.ProfileCount))
		{
			OutError = TEXT("Hydrology runoff edge split contradicts canonical parent direction");
			return EVoxelHydrologyClosureRead::Failed;
		}
		OutContribution.RainWeight = bFirst ? Split.FirstRainWeight : Split.SecondRainWeight;
		OutContribution.MaxChildRainWeight = OutContribution.RainWeight;
		OutContribution.Child = OtherCell;
		OutContribution.bHasChild = bOtherIntoEdge;
		OutContribution.InputFingerprint = VoxelGeneration::Mix(OtherFingerprint ^ HydrologyRunoffControlFingerprint(Other) ^ uint64(EdgeIndex));
		return EVoxelHydrologyClosureRead::Ready;
	}
	const int32 Face = Node.Slot - Facts.EdgeCount;
	if (!(Facts.BoundaryFaces & (1 << Face))) return EVoxelHydrologyClosureRead::Ready;
	static const FIntPoint Offsets[] = {FIntPoint(-1, 0), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(0, 1)};
	const int64 X = int64(Node.Cell.X) + Offsets[Face].X;
	const int64 Y = int64(Node.Cell.Y) + Offsets[Face].Y;
	if (X < Minimum.X || Y < Minimum.Y || X > Maximum.X || Y > Maximum.Y) return EVoxelHydrologyClosureRead::Ready;
	const FIntPoint OtherCell{int32(X), int32(Y)};
	FVoxelHydrologyNetworkCell Other;
	uint64 OtherFingerprint = 0;
	const auto OtherRead = ReadControl(OtherCell, InReadRegion, InReadControl, Other, OtherFingerprint, OutError);
	if (OtherRead != EVoxelHydrologyClosureRead::Ready) return OtherRead;
	const bool bChild = Other.bHasParent && Other.ParentCell == Node.Cell;
	if (bChild && ((Node.Control.bHasParent && Node.Control.ParentCell == OtherCell) ||
		Node.Control.Potential.SpillZ > Other.Potential.SpillZ ||
		(Node.Control.Potential.SpillZ == Other.Potential.SpillZ &&
			(Node.Control.Potential.PlateauDistance >= MAX_int64 - 1 || Other.Potential.PlateauDistance != Node.Control.Potential.PlateauDistance + 1))))
	{
		OutError = TEXT("Hydrology runoff cross region edge is not descending");
		return EVoxelHydrologyClosureRead::Failed;
	}
	OutContribution.Child = OtherCell;
	OutContribution.bHasChild = bChild;
	OutContribution.bExternal = true;
	OutContribution.InputFingerprint = VoxelGeneration::Mix(OtherFingerprint ^ HydrologyRunoffControlFingerprint(Other));
	return EVoxelHydrologyClosureRead::Ready;
}

bool FVoxelHydrologyNetworkRunoffBuild::AppendNode(const FIntPoint& InCell, int32& OutIndex, FString& OutError)
{
	if (const int32* Existing = ByCell.Find(InCell))
	{
		OutIndex = *Existing;
		return true;
	}
	if (!Nodes.ReserveOne(OutError)) return false;
	if (ByCell.Num() == MapCapacity)
	{
		const int32 Capacity = HydrologyRunoffCapacity(MapCapacity);
		if (!VoxelGenerationAllocation::RetainMap(ByCell, Capacity, OutError)) return false;
		MapCapacity = Capacity;
	}
	FNode Node;
	Node.Cell = InCell;
	OutIndex = Nodes.Add(Node);
	ByCell.Add(InCell, OutIndex);
	return true;
}

bool FVoxelHydrologyNetworkRunoffBuild::ReserveStack(FString& OutError)
{
	if (Stack.Num() == MAX_int32)
	{
		OutError = TEXT("Oversized: hydrology upstream dependency depth");
		return false;
	}
	return Stack.Num() < Stack.Max() || VoxelGenerationAllocation::RetainArray(Stack, HydrologyRunoffCapacity(Stack.Max()), OutError);
}

EVoxelHydrologyRegionStep FVoxelHydrologyNetworkRunoffBuild::Failure(FString& OutError) const
{
	if (OutError == TEXT("Canceled")) return EVoxelHydrologyRegionStep::Canceled;
	if (OutError.StartsWith(TEXT("CapacityBlocked"))) return EVoxelHydrologyRegionStep::CapacityBlocked;
	if (OutError.IsEmpty()) OutError = TEXT("HydrologyComponentConflict: invalid network runoff");
	return EVoxelHydrologyRegionStep::Failed;
}

EVoxelHydrologyRegionStep FVoxelHydrologyNetworkRunoffBuild::ReadFailure(const EVoxelHydrologyClosureRead InRead, FString& OutError) const
{
	return InRead == EVoxelHydrologyClosureRead::Pending ? EVoxelHydrologyRegionStep::NeedInput : Failure(OutError);
}

EVoxelHydrologyRegionStep FVoxelHydrologyNetworkRunoffBuild::Advance(const uint64 InGeneration,
	FReadRegion InReadRegion, const int32 InWorkBudget, FString& OutError, const TAtomic<bool>* InCancel)
{
	const auto ReadCanonical = [&](const FIntPoint& InCell, FVoxelHydrologyNetworkCell& OutControl, FString& InError)
	{
		return ResolveControl(InCell, InReadRegion, FMath::Min(256, FMath::Max(1, InWorkBudget)), OutControl, InError, InCancel);
	};
	return Advance(InGeneration, InReadRegion, ReadCanonical, InWorkBudget, OutError, InCancel);
}

EVoxelHydrologyClosureRead FVoxelHydrologyNetworkRunoffBuild::ResolveControl(const FIntPoint& InCell,
	FReadRegion InReadRegion, int32 InWorkBudget, FVoxelHydrologyNetworkCell& OutControl,
	FString& OutError, const TAtomic<bool>* InCancel)
{
	if (const int32* Existing = ByCell.Find(InCell))
	{
		if (Nodes[*Existing].bControlReady)
		{
			OutControl = Nodes[*Existing].Control;
			OutError.Reset();
			return EVoxelHydrologyClosureRead::Ready;
		}
	}
	if (!ControlSearch.IsSet())
	{
		const FVoxelHydrologyRegionGraph* Graph = nullptr;
		const auto Read = ReadGraph(RegionFor(InCell), InReadRegion, Graph, OutError);
		if (Read != EVoxelHydrologyClosureRead::Ready) return Read;
		const int32 Index = Graph->FindNodeIndex(InCell);
		if (!Graph->Nodes.IsValidIndex(Index))
		{
			OutError = TEXT("Hydrology macro control requires a retained frozen node");
			return EVoxelHydrologyClosureRead::Failed;
		}
		const auto& Facts = Graph->Nodes[Index];
		if (Facts.IsReceiver())
		{
			OutControl = {InCell, {Facts.WaterZ, 0}, FIntPoint::ZeroValue, false};
			OutError.Reset();
			return EVoxelHydrologyClosureRead::Ready;
		}
		ControlSearch.Emplace();
		const bool bInitialized = RegionFor(InCell) == Result.Coordinate
			? ControlSearch->InitializeRegion(InCell, CellSize, RegionSide, Result.Generation, OutError)
			: ControlSearch->Initialize(InCell, CellSize, RegionSide, Result.Generation, OutError);
		if (!bInitialized)
		{
			ControlSearch.Reset();
			return EVoxelHydrologyClosureRead::Failed;
		}
		ControlCopyCursor = 0;
	}
	const auto Step = ControlSearch->Advance(Result.Generation, InReadRegion, InWorkBudget, OutError, InCancel);
	if (Step == EVoxelHydrologyNetworkStep::Yielded || Step == EVoxelHydrologyNetworkStep::NeedInput)
	{
		return EVoxelHydrologyClosureRead::Pending;
	}
	if (Step != EVoxelHydrologyNetworkStep::Ready)
	{
		if (Step == EVoxelHydrologyNetworkStep::NoReceiver)
		{
			OutError = TEXT("HydrologyComponentConflict: no physical receiver for macro upstream control");
		}
		return EVoxelHydrologyClosureRead::Failed;
	}
	const auto& Controls = ControlSearch->GetResult().Cells;
	while (ControlCopyCursor < Controls.Num() && InWorkBudget-- > 0)
	{
		const FVoxelHydrologyNetworkCell Control = Controls[ControlCopyCursor];
		int32 Index = INDEX_NONE;
		if (!AppendNode(Control.Cell, Index, OutError)) return EVoxelHydrologyClosureRead::Failed;
		auto& Node = Nodes[Index];
		if (Node.bControlReady && Node.Control != Control)
		{
			OutError = TEXT("HydrologyComponentConflict: overlapping macro network controls disagree");
			return EVoxelHydrologyClosureRead::Failed;
		}
		Node.Control = Control;
		Node.bControlReady = true;
		++ControlCopyCursor;
	}
	if (ControlCopyCursor != Controls.Num())
	{
		OutError = TEXT("Yielded: macro network control publication");
		return EVoxelHydrologyClosureRead::Pending;
	}
	const uint64 Bytes = ControlSearch->GetAllocatedBytes();
	ControlSearch.Reset();
	if (auto* Allocation = FVoxelGenerationAllocationScope::Get()) Allocation->ReleaseRetainedBytes(Bytes);
	if (const int32* Existing = ByCell.Find(InCell))
	{
		if (Nodes[*Existing].bControlReady)
		{
			OutControl = Nodes[*Existing].Control;
			OutError.Reset();
			return EVoxelHydrologyClosureRead::Ready;
		}
	}
	OutError = TEXT("Yielded: another macro control requires its own physical outlet closure");
	return EVoxelHydrologyClosureRead::Pending;
}

EVoxelHydrologyRegionStep FVoxelHydrologyNetworkRunoffBuild::Advance(const uint64 InGeneration,
	FReadRegion InReadRegion, FReadControl InReadControl, int32 InWorkBudget, FString& OutError, const TAtomic<bool>* InCancel)
{
	OutError.Reset();
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return EVoxelHydrologyRegionStep::Canceled;
	}
	if (Stage == EStage::None || Stage == EStage::Taken || InGeneration != Result.Generation)
	{
		OutError = TEXT("Invalid hydrology runoff generation or lifetime");
		return Failure(OutError);
	}
	while (InWorkBudget-- > 0)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return EVoxelHydrologyRegionStep::Canceled;
		}
		if (Stage == EStage::Seeds)
		{
			const FVoxelHydrologyRegionGraph* Graph = nullptr;
			const auto Read = ReadGraph(Result.Coordinate, InReadRegion, Graph, OutError);
			if (Read != EVoxelHydrologyClosureRead::Ready) return ReadFailure(Read, OutError);
			if (!Result.GraphFingerprint)
			{
				if (!VoxelGenerationAllocation::RetainArray(Result.Cells, Graph->Nodes.Num(), OutError)) return Failure(OutError);
				Result.GraphFingerprint = Graph->Fingerprint;
				Result.InputFingerprint = VoxelGeneration::Mix(Graph->InputFingerprint ^ Result.Generation);
			}
			if (Graph->Fingerprint != Result.GraphFingerprint)
			{
				OutError = TEXT("Hydrology runoff target region changed");
				return Failure(OutError);
			}
			if (SeedCursor == Graph->Nodes.Num())
			{
				Stage = EStage::Hash;
				Result.Fingerprint = VoxelGeneration::Mix(Result.InputFingerprint ^ Result.GraphFingerprint);
				continue;
			}
			const FIntPoint Cell = Graph->Nodes[SeedCursor].Cell;
			if (!AppendNode(Cell, Root, OutError) || !ReserveStack(OutError)) return Failure(OutError);
			Stack.Add(Root);
			Stage = EStage::Solve;
		}
		else if (Stage == EStage::Solve)
		{
			if (Stack.IsEmpty())
			{
				const FNode& Node = Nodes[Root];
				check(Node.State == ENodeState::Done && Result.Cells.Num() < Result.Cells.Max());
				Result.Cells.Add({Node.Control, Node.Accumulation, Node.IncomingRainWeight, Node.MaxChildRainWeight});
				Root = INDEX_NONE;
				++SeedCursor;
				Stage = EStage::Seeds;
				continue;
			}
			const int32 Current = Stack.Last();
			if (Nodes[Current].State == ENodeState::Done) { Stack.Pop(EAllowShrinking::No); continue; }
			if (Nodes[Current].State == ENodeState::None)
			{
				FVoxelHydrologyNetworkCell Control;
				uint64 Fingerprint = 0;
				const auto Read = ReadControl(Nodes[Current].Cell, InReadRegion, InReadControl, Control, Fingerprint, OutError);
				if (Read != EVoxelHydrologyClosureRead::Ready) return ReadFailure(Read, OutError);
				const FVoxelHydrologyRegionGraph* Graph = nullptr;
				const auto Again = ReadGraph(RegionFor(Control.Cell), InReadRegion, Graph, OutError);
				if (Again != EVoxelHydrologyClosureRead::Ready) return ReadFailure(Again, OutError);
				if (Graph->Fingerprint != Fingerprint)
				{
					OutError = TEXT("Hydrology runoff region changed during admission");
					return Failure(OutError);
				}
				auto& Node = Nodes[Current];
				Node.LocalIndex = Graph->FindNodeIndex(Control.Cell);
				Node.Control = Control;
				Node.bControlReady = true;
				Node.GraphFingerprint = Fingerprint;
				Node.Accumulation = Graph->Nodes[Node.LocalIndex].RainWeight;
				const FIntPoint Local = Control.Cell - Graph->Minimum;
				Node.MaxChildRainWeight = Graph->GetMaxPrunedChildRainWeight(uint32(Local.X + Local.Y * Graph->Width));
				Node.State = ENodeState::Visiting;
				Result.InputFingerprint = VoxelGeneration::Mix(Result.InputFingerprint ^ Fingerprint ^ HydrologyRunoffControlFingerprint(Control));
				continue;
			}
			if (Nodes[Current].Child != INDEX_NONE)
			{
				auto& Node = Nodes[Current];
				const auto& Child = Nodes[Node.Child];
				check(Child.State == ENodeState::Done);
				if (MAX_uint64 - Node.Accumulation < Child.Accumulation ||
					(Node.bExternalChild && MAX_uint64 - Node.IncomingRainWeight < Child.Accumulation))
				{
					OutError = TEXT("Oversized: upstream rainfall accumulation");
					return Failure(OutError);
				}
				Node.Accumulation += Child.Accumulation;
				if (MAX_uint64 - Node.ChildLocalRainWeight < Child.Accumulation)
				{
					OutError = TEXT("Oversized: immediate upstream child rainfall");
					return Failure(OutError);
				}
				Node.MaxChildRainWeight = FMath::Max(Node.MaxChildRainWeight, Node.ChildLocalRainWeight + Child.Accumulation);
				if (Node.bExternalChild) Node.IncomingRainWeight += Child.Accumulation;
				Node.Child = INDEX_NONE;
				Node.bExternalChild = false;
				Node.ChildLocalRainWeight = 0;
				++Node.Slot;
				continue;
			}
			FContribution Contribution;
			const auto Read = ReadContribution(Current, InReadRegion, InReadControl, Contribution, OutError);
			if (Read != EVoxelHydrologyClosureRead::Ready) return ReadFailure(Read, OutError);
			if (Contribution.bAtEnd)
			{
				Nodes[Current].State = ENodeState::Done;
				Stack.Pop(EAllowShrinking::No);
				continue;
			}
			if (MAX_uint64 - Nodes[Current].Accumulation < Contribution.RainWeight)
			{
				OutError = TEXT("Oversized: regional edge rainfall");
				return Failure(OutError);
			}
			int32 Child = INDEX_NONE;
			if (Contribution.bHasChild)
			{
				if (!AppendNode(Contribution.Child, Child, OutError)) return Failure(OutError);
				if (Nodes[Child].State == ENodeState::Visiting)
				{
					OutError = TEXT("HydrologyComponentConflict: cyclic upstream macro dependency");
					return Failure(OutError);
				}
				if (!ReserveStack(OutError)) return Failure(OutError);
			}
			auto& Node = Nodes[Current];
			Node.Accumulation += Contribution.RainWeight;
			Result.InputFingerprint = VoxelGeneration::Mix(Result.InputFingerprint ^ Contribution.InputFingerprint);
			if (Child != INDEX_NONE)
			{
				Node.Child = Child;
				Node.bExternalChild = Contribution.bExternal;
				Node.ChildLocalRainWeight = Contribution.MaxChildRainWeight;
				Stack.Add(Child);
			}
			else
			{
				Node.MaxChildRainWeight = FMath::Max(Node.MaxChildRainWeight, Contribution.MaxChildRainWeight);
				++Node.Slot;
			}
		}
		else if (Stage == EStage::Hash)
		{
			if (HashCursor < Result.Cells.Num())
			{
				const auto& Boundary = Result.Cells[HashCursor++];
				Result.Fingerprint = VoxelGeneration::Mix(Result.Fingerprint ^ HydrologyRunoffControlFingerprint(Boundary.Control));
				Result.Fingerprint = VoxelGeneration::Mix(Result.Fingerprint ^ Boundary.Accumulation ^ VoxelGeneration::Mix(Boundary.IncomingRainWeight));
				Result.Fingerprint = VoxelGeneration::Mix(Result.Fingerprint ^ Boundary.MaxChildRainWeight);
				continue;
			}
			if (!Result.Fingerprint) Result.Fingerprint = 1;
			FreeScratch();
			Stage = EStage::Ready;
			return EVoxelHydrologyRegionStep::Ready;
		}
		else if (Stage == EStage::Ready) return EVoxelHydrologyRegionStep::Ready;
	}
	return Stage == EStage::Ready ? EVoxelHydrologyRegionStep::Ready : EVoxelHydrologyRegionStep::Yielded;
}

void FVoxelHydrologyNetworkRunoffBuild::FreeScratch()
{
	if (ControlSearch.IsSet())
	{
		const uint64 Bytes = ControlSearch->GetAllocatedBytes();
		ControlSearch.Reset();
		if (auto* Allocation = FVoxelGenerationAllocationScope::Get()) Allocation->ReleaseRetainedBytes(Bytes);
	}
	Nodes.FreeBacking();
	VoxelGenerationAllocation::FreeRetainedMap(ByCell);
	VoxelGenerationAllocation::FreeRetainedArray(Stack);
	MapCapacity = 0;
}

const FVoxelHydrologyNetworkRunoff& FVoxelHydrologyNetworkRunoffBuild::GetResult() const
{
	check(Stage == EStage::Ready);
	return Result;
}

FVoxelHydrologyNetworkRunoff FVoxelHydrologyNetworkRunoffBuild::TakeResult()
{
	check(Stage == EStage::Ready);
	Stage = EStage::Taken;
	return MoveTemp(Result);
}

uint64 FVoxelHydrologyNetworkRunoffBuild::GetAllocatedBytes() const
{
	return Result.GetAllocatedBytes() + Nodes.GetAllocatedBytes() + (ControlSearch.IsSet() ? ControlSearch->GetAllocatedBytes() : 0) +
		VoxelGenerationAllocation::AllocatedBytes(ByCell) +
		VoxelGenerationAllocation::AllocatedBytes(Stack);
}

int32 FVoxelHydrologyNetworkRunoffBuild::GetNodeCount() const
{
	return Nodes.Num();
}
