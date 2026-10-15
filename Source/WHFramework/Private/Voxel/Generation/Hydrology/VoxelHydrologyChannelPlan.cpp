#include "Voxel/Generation/Hydrology/VoxelHydrologyChannelPlan.h"

#include "Voxel/Generation/VoxelGenerationMath.h"

namespace
{
	int32 HydrologyChannelCapacity(const int32 InCapacity)
	{
		return int32(FMath::Min<int64>(MAX_int32, FMath::Max<int64>(16, int64(InCapacity) * 2)));
	}
}

const FVoxelHydrologyChannelCell* FVoxelHydrologyChannelPlan::Find(const FIntPoint& InCell) const
{
	int32 Lower = 0;
	int32 Upper = Cells.Num();
	while (Lower < Upper)
	{
		const int32 Middle = Lower + (Upper - Lower) / 2;
		const FIntPoint Cell = Cells[Middle].Cell;
		if (Cell.Y < InCell.Y || (Cell.Y == InCell.Y && Cell.X < InCell.X)) Lower = Middle + 1;
		else Upper = Middle;
	}
	return Cells.IsValidIndex(Lower) && Cells[Lower].Cell == InCell ? &Cells[Lower] : nullptr;
}

uint64 FVoxelHydrologyChannelPlan::GetAllocatedBytes() const
{
	return VoxelGenerationAllocation::AllocatedBytes(Cells);
}

FVoxelHydrologyChannelPlanBuild::FVoxelHydrologyChannelPlanBuild()
{
}

FVoxelHydrologyChannelPlanBuild::~FVoxelHydrologyChannelPlanBuild()
{
}

bool FVoxelHydrologyChannelPlanBuild::Initialize(const FIntPoint& InCoordinate, const int32 InCellSize,
	const int32 InRegionSide, const uint64 InGeneration, FString& OutError)
{
	if (Stage != EStage::None || InCellSize <= 0 || InRegionSide <= 0 || InRegionSide > 4096 || !InGeneration)
	{
		OutError = TEXT("Invalid hydrology channel plan generation or dimensions");
		return false;
	}
	const int64 Lower = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMin, InCellSize);
	const int64 Upper = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMaxExclusive - 1, InCellSize);
	if (int64(InCoordinate.X) * InRegionSide > Upper || int64(InCoordinate.Y) * InRegionSide > Upper ||
		(int64(InCoordinate.X) + 1) * InRegionSide <= Lower || (int64(InCoordinate.Y) + 1) * InRegionSide <= Lower)
	{
		OutError = TEXT("Hydrology channel region is outside the physical world");
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

FIntPoint FVoxelHydrologyChannelPlanBuild::RegionFor(const FIntPoint& InCell) const
{
	return FIntPoint(VoxelGeneration::FloorDivide(InCell.X, RegionSide), VoxelGeneration::FloorDivide(InCell.Y, RegionSide));
}

EVoxelHydrologyClosureRead FVoxelHydrologyChannelPlanBuild::ReadInputs(const FIntPoint& InCoordinate,
	FReadRegion InReadRegion, FVoxelHydrologyChannelInputs& OutInputs, FString& OutError) const
{
	OutInputs = {};
	const auto Read = InReadRegion(InCoordinate, OutInputs, OutError);
	if (Read != EVoxelHydrologyClosureRead::Ready) return Read;
	if (!OutInputs.Graph || !OutInputs.Runoff || !OutInputs.Sources)
	{
		OutError = TEXT("Hydrology channel plan requires all frozen regional inputs");
		return EVoxelHydrologyClosureRead::Failed;
	}
	if (!OutInputs.Graph->ValidateCore(InCoordinate, CellSize, RegionSide, Result.Generation, OutError)) return EVoxelHydrologyClosureRead::Failed;
	if (OutInputs.Runoff->Generation != Result.Generation || OutInputs.Runoff->GraphFingerprint != OutInputs.Graph->Fingerprint ||
		!OutInputs.Runoff->Fingerprint || OutInputs.Sources->Generation != Result.Generation ||
		OutInputs.Sources->GraphFingerprint != OutInputs.Graph->Fingerprint || OutInputs.Sources->RunoffFingerprint != OutInputs.Runoff->Fingerprint ||
		!OutInputs.Sources->Fingerprint)
	{
		OutError = TEXT("Hydrology channel plan regional input identities disagree");
		return EVoxelHydrologyClosureRead::Failed;
	}
	return EVoxelHydrologyClosureRead::Ready;
}

EVoxelHydrologyClosureRead FVoxelHydrologyChannelPlanBuild::ReadContribution(const int32 InNode,
	FReadRegion InReadRegion, FContribution& OutContribution, FString& OutError) const
{
	OutContribution = {};
	const FNode Node = Nodes[InNode];
	FVoxelHydrologyChannelInputs Inputs;
	const auto Read = ReadInputs(RegionFor(Node.Cell), InReadRegion, Inputs, OutError);
	if (Read != EVoxelHydrologyClosureRead::Ready) return Read;
	if (Inputs.Graph->Fingerprint != Node.GraphFingerprint || Inputs.Runoff->Fingerprint != Node.RunoffFingerprint ||
		Inputs.Sources->Fingerprint != Node.SourceFingerprint)
	{
		OutError = TEXT("Hydrology channel plan source changed after admission");
		return EVoxelHydrologyClosureRead::Failed;
	}
	const auto Facts = Inputs.Graph->Nodes[Node.LocalIndex];
	if (Node.Slot >= Facts.EdgeCount + 4)
	{
		OutContribution.bAtEnd = true;
		return EVoxelHydrologyClosureRead::Ready;
	}
	if (Node.Slot < Facts.EdgeCount)
	{
		const int32 EdgeIndex = Facts.Edges[Node.Slot];
		const auto& Edge = Inputs.Graph->Edges[EdgeIndex];
		const bool bFirst = Edge.First == Node.LocalIndex;
		const FIntPoint OtherCell = Inputs.Graph->Nodes[bFirst ? Edge.Second : Edge.First].Cell;
		const auto* Other = Inputs.Runoff->Find(OtherCell);
		if (!Other)
		{
			OutError = TEXT("Hydrology channel plan has no resolved edge endpoint");
			return EVoxelHydrologyClosureRead::Failed;
		}
		FVoxelHydrologyRegionSplit Split;
		if (!Inputs.Graph->SplitEdgeRain(EdgeIndex, bFirst ? Node.Control.Potential : Other->Control.Potential,
			bFirst ? Other->Control.Potential : Node.Control.Potential, Split, OutError)) return EVoxelHydrologyClosureRead::Failed;
		if (Edge.First == Edge.Second)
		{
			for (int32 Previous = 0; Previous < Node.Slot; ++Previous)
			{
				if (Facts.Edges[Previous] == EdgeIndex) return EVoxelHydrologyClosureRead::Ready;
			}
			OutContribution.bActive = Inputs.Sources->HasSourceInRange((1u << 30) + uint32(Edge.ProfileOffset),
				(1u << 30) + uint32(Edge.ProfileOffset + Edge.ProfileCount));
			return EVoxelHydrologyClosureRead::Ready;
		}
		const int32 Begin = bFirst ? 0 : Split.FirstProfileCount;
		const int32 End = bFirst ? Split.FirstProfileCount : Edge.ProfileCount;
		OutContribution.bActive = Inputs.Sources->HasSourceInRange((1u << 30) + uint32(Edge.ProfileOffset + Begin),
			(1u << 30) + uint32(Edge.ProfileOffset + End));
		OutContribution.bHasChild = Other->Control.bHasParent && Other->Control.ParentCell == (bFirst ? Edge.LastStep : Edge.FirstStep);
		OutContribution.Child = OtherCell;
		OutContribution.Fingerprint = VoxelGeneration::Mix(Inputs.Sources->Fingerprint ^ uint64(EdgeIndex));
		return EVoxelHydrologyClosureRead::Ready;
	}
	const int32 Face = Node.Slot - Facts.EdgeCount;
	if (!(Facts.BoundaryFaces & (1 << Face))) return EVoxelHydrologyClosureRead::Ready;
	static const FIntPoint Offsets[] = {FIntPoint(-1, 0), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(0, 1)};
	const int64 X = int64(Node.Cell.X) + Offsets[Face].X;
	const int64 Y = int64(Node.Cell.Y) + Offsets[Face].Y;
	if (X < Minimum.X || Y < Minimum.Y || X > Maximum.X || Y > Maximum.Y) return EVoxelHydrologyClosureRead::Ready;
	const FIntPoint OtherCell{int32(X), int32(Y)};
	FVoxelHydrologyChannelInputs OtherInputs;
	const auto OtherRead = ReadInputs(RegionFor(OtherCell), InReadRegion, OtherInputs, OutError);
	if (OtherRead != EVoxelHydrologyClosureRead::Ready) return OtherRead;
	const auto* Other = OtherInputs.Runoff->Find(OtherCell);
	if (!Other)
	{
		OutError = TEXT("Hydrology channel plan omits a physical boundary control");
		return EVoxelHydrologyClosureRead::Failed;
	}
	OutContribution.Child = OtherCell;
	OutContribution.bHasChild = Other->Control.bHasParent && Other->Control.ParentCell == Node.Cell;
	OutContribution.bExternal = true;
	OutContribution.Fingerprint = VoxelGeneration::Mix(OtherInputs.Sources->Fingerprint ^ OtherInputs.Runoff->Fingerprint);
	return EVoxelHydrologyClosureRead::Ready;
}

bool FVoxelHydrologyChannelPlanBuild::AppendNode(const FIntPoint& InCell, int32& OutIndex, FString& OutError)
{
	if (const int32* Existing = ByCell.Find(InCell)) { OutIndex = *Existing; return true; }
	if (!Nodes.ReserveOne(OutError)) return false;
	if (ByCell.Num() == MapCapacity)
	{
		const int32 Capacity = HydrologyChannelCapacity(MapCapacity);
		if (!VoxelGenerationAllocation::RetainMap(ByCell, Capacity, OutError)) return false;
		MapCapacity = Capacity;
	}
	FNode Node;
	Node.Cell = InCell;
	OutIndex = Nodes.Add(Node);
	ByCell.Add(InCell, OutIndex);
	return true;
}

bool FVoxelHydrologyChannelPlanBuild::ReserveStack(FString& OutError)
{
	if (Stack.Num() == MAX_int32)
	{
		OutError = TEXT("Oversized: hydrology channel dependency depth");
		return false;
	}
	return Stack.Num() < Stack.Max() || VoxelGenerationAllocation::RetainArray(Stack, HydrologyChannelCapacity(Stack.Max()), OutError);
}

EVoxelHydrologyRegionStep FVoxelHydrologyChannelPlanBuild::Failure(FString& OutError) const
{
	if (OutError == TEXT("Canceled")) return EVoxelHydrologyRegionStep::Canceled;
	if (OutError.StartsWith(TEXT("CapacityBlocked"))) return EVoxelHydrologyRegionStep::CapacityBlocked;
	if (OutError.IsEmpty()) OutError = TEXT("HydrologyComponentConflict: invalid channel activation");
	return EVoxelHydrologyRegionStep::Failed;
}

EVoxelHydrologyRegionStep FVoxelHydrologyChannelPlanBuild::ReadFailure(const EVoxelHydrologyClosureRead InRead, FString& OutError) const
{
	return InRead == EVoxelHydrologyClosureRead::Pending ? EVoxelHydrologyRegionStep::NeedInput : Failure(OutError);
}

EVoxelHydrologyRegionStep FVoxelHydrologyChannelPlanBuild::Advance(const uint64 InGeneration,
	FReadRegion InReadRegion, int32 InWorkBudget, FString& OutError, const TAtomic<bool>* InCancel)
{
	OutError.Reset();
	if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return EVoxelHydrologyRegionStep::Canceled; }
	if (Stage == EStage::None || Stage == EStage::Taken || InGeneration != Result.Generation)
	{
		OutError = TEXT("Invalid hydrology channel plan lifetime or generation");
		return Failure(OutError);
	}
	while (InWorkBudget-- > 0)
	{
		if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return EVoxelHydrologyRegionStep::Canceled; }
		if (Stage == EStage::Seeds)
		{
			FVoxelHydrologyChannelInputs Inputs;
			const auto Read = ReadInputs(Result.Coordinate, InReadRegion, Inputs, OutError);
			if (Read != EVoxelHydrologyClosureRead::Ready) return ReadFailure(Read, OutError);
			if (!Result.GraphFingerprint)
			{
				if (!VoxelGenerationAllocation::RetainArray(Result.Cells, Inputs.Graph->Nodes.Num(), OutError)) return Failure(OutError);
				Result.GraphFingerprint = Inputs.Graph->Fingerprint;
				Result.RunoffFingerprint = Inputs.Runoff->Fingerprint;
				Result.SourceFingerprint = Inputs.Sources->Fingerprint;
				Result.Fingerprint = VoxelGeneration::Mix(Result.Generation ^ Result.GraphFingerprint ^ Result.RunoffFingerprint ^ Result.SourceFingerprint);
			}
			if (Result.GraphFingerprint != Inputs.Graph->Fingerprint || Result.RunoffFingerprint != Inputs.Runoff->Fingerprint ||
				Result.SourceFingerprint != Inputs.Sources->Fingerprint)
			{
				OutError = TEXT("Hydrology channel plan target changed");
				return Failure(OutError);
			}
			if (SeedCursor == Inputs.Graph->Nodes.Num()) { Stage = EStage::Hash; continue; }
			if (!AppendNode(Inputs.Graph->Nodes[SeedCursor].Cell, Root, OutError) || !ReserveStack(OutError)) return Failure(OutError);
			Stack.Add(Root);
			Stage = EStage::Solve;
		}
		else if (Stage == EStage::Solve)
		{
			if (Stack.IsEmpty())
			{
				const auto& Node = Nodes[Root];
				check(Node.State == ENodeState::Done && Result.Cells.Num() < Result.Cells.Max());
				Result.Cells.Add({Node.Cell, Node.bActive, Node.bIncomingActive});
				Root = INDEX_NONE;
				++SeedCursor;
				Stage = EStage::Seeds;
				continue;
			}
			const int32 Current = Stack.Last();
			if (Nodes[Current].State == ENodeState::Done) { Stack.Pop(EAllowShrinking::No); continue; }
			if (Nodes[Current].State == ENodeState::None)
			{
				FVoxelHydrologyChannelInputs Inputs;
				const auto Read = ReadInputs(RegionFor(Nodes[Current].Cell), InReadRegion, Inputs, OutError);
				if (Read != EVoxelHydrologyClosureRead::Ready) return ReadFailure(Read, OutError);
				auto& Node = Nodes[Current];
				Node.LocalIndex = Inputs.Graph->FindNodeIndex(Node.Cell);
				const auto* Flow = Inputs.Runoff->Find(Node.Cell);
				if (!Flow || Node.LocalIndex == INDEX_NONE)
				{
					OutError = TEXT("Hydrology channel plan requires an actual critical control");
					return Failure(OutError);
				}
				Node.Control = Flow->Control;
				Node.GraphFingerprint = Inputs.Graph->Fingerprint;
				Node.RunoffFingerprint = Inputs.Runoff->Fingerprint;
				Node.SourceFingerprint = Inputs.Sources->Fingerprint;
				Node.bActive = Inputs.Sources->HasSourceInRange(uint32(Node.LocalIndex), uint32(Node.LocalIndex + 1));
				Node.State = ENodeState::Visiting;
				Result.Fingerprint = VoxelGeneration::Mix(Result.Fingerprint ^ Node.SourceFingerprint ^ Node.RunoffFingerprint ^
					((uint64(uint32(Node.Cell.X)) << 32) | uint32(Node.Cell.Y)));
				continue;
			}
			if (Nodes[Current].Child != INDEX_NONE)
			{
				auto& Node = Nodes[Current];
				const auto& Child = Nodes[Node.Child];
				check(Child.State == ENodeState::Done);
				Node.bActive |= Child.bActive;
				if (Node.bExternalChild) Node.bIncomingActive |= Child.bActive;
				Node.Child = INDEX_NONE;
				Node.bExternalChild = false;
				++Node.Slot;
				continue;
			}
			FContribution Contribution;
			const auto Read = ReadContribution(Current, InReadRegion, Contribution, OutError);
			if (Read != EVoxelHydrologyClosureRead::Ready) return ReadFailure(Read, OutError);
			if (Contribution.bAtEnd) { Nodes[Current].State = ENodeState::Done; Stack.Pop(EAllowShrinking::No); continue; }
			int32 Child = INDEX_NONE;
			if (Contribution.bHasChild)
			{
				if (!AppendNode(Contribution.Child, Child, OutError)) return Failure(OutError);
				if (Nodes[Child].State == ENodeState::Visiting)
				{
					OutError = TEXT("HydrologyComponentConflict: channel dependency has a cycle");
					return Failure(OutError);
				}
				if (!ReserveStack(OutError)) return Failure(OutError);
			}
			auto& Node = Nodes[Current];
			Node.bActive |= Contribution.bActive;
			Result.Fingerprint = VoxelGeneration::Mix(Result.Fingerprint ^ Contribution.Fingerprint);
			if (Child != INDEX_NONE)
			{
				Node.Child = Child;
				Node.bExternalChild = Contribution.bExternal;
				Stack.Add(Child);
			}
			else ++Node.Slot;
		}
		else if (Stage == EStage::Hash)
		{
			if (HashCursor < Result.Cells.Num())
			{
				const auto& Cell = Result.Cells[HashCursor++];
				Result.Fingerprint = VoxelGeneration::Mix(Result.Fingerprint ^ ((uint64(uint32(Cell.Cell.X)) << 32) |
					uint32(Cell.Cell.Y)) ^ uint64(Cell.bActive) ^ (uint64(Cell.bIncomingActive) << 1));
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

void FVoxelHydrologyChannelPlanBuild::FreeScratch()
{
	Nodes.FreeBacking();
	VoxelGenerationAllocation::FreeRetainedMap(ByCell);
	VoxelGenerationAllocation::FreeRetainedArray(Stack);
	MapCapacity = 0;
}

const FVoxelHydrologyChannelPlan& FVoxelHydrologyChannelPlanBuild::GetResult() const
{
	check(Stage == EStage::Ready);
	return Result;
}

FVoxelHydrologyChannelPlan FVoxelHydrologyChannelPlanBuild::TakeResult()
{
	check(Stage == EStage::Ready);
	Stage = EStage::Taken;
	return MoveTemp(Result);
}

uint64 FVoxelHydrologyChannelPlanBuild::GetAllocatedBytes() const
{
	return Result.GetAllocatedBytes() + Nodes.GetAllocatedBytes() + VoxelGenerationAllocation::AllocatedBytes(ByCell) + VoxelGenerationAllocation::AllocatedBytes(Stack);
}
