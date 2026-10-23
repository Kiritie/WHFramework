#include "Voxel/Generation/Hydrology/VoxelHydrologyRegionGraph.h"

#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "HAL/PlatformTime.h"

uint64 FVoxelHydrologyRegionGraph::GetAllocatedBytes() const
{
	return VoxelGenerationAllocation::AllocatedBytes(Nodes) + VoxelGenerationAllocation::AllocatedBytes(Edges) +
		VoxelGenerationAllocation::AllocatedBytes(Profiles) + VoxelGenerationAllocation::AllocatedBytes(AnchorRuns) +
		VoxelGenerationAllocation::AllocatedBytes(ForestNeighbors) + VoxelGenerationAllocation::AllocatedBytes(CellLocations) +
		VoxelGenerationAllocation::AllocatedBytes(PrunedCells) + VoxelGenerationAllocation::AllocatedBytes(PrunedByParent);
}

bool FVoxelHydrologyRegionGraph::HasEquivalentOutput(const FVoxelHydrologyRegionGraph& InOther) const
{
	return Minimum == InOther.Minimum && Width == InOther.Width && Height == InOther.Height && CellSize == InOther.CellSize &&
		Generation == InOther.Generation && InputFingerprint == InOther.InputFingerprint && Fingerprint == InOther.Fingerprint &&
		TotalLandRainWeight == InOther.TotalLandRainWeight && Nodes == InOther.Nodes && Edges == InOther.Edges &&
		Profiles == InOther.Profiles && AnchorRuns == InOther.AnchorRuns && ForestNeighbors == InOther.ForestNeighbors &&
		CellLocations == InOther.CellLocations && PrunedCells == InOther.PrunedCells && PrunedByParent == InOther.PrunedByParent;
}

bool FVoxelHydrologyRegionGraph::ValidateCore(const FIntPoint& InCoordinate, const int32 InCellSize,
	const int32 InRegionSide, const uint64 InGeneration, FString& OutError) const
{
	if (InCellSize <= 0 || InRegionSide <= 0 || InRegionSide > 4096 || !InGeneration)
	{
		OutError = TEXT("Invalid hydrology network region dimensions or generation");
		return false;
	}
	const int64 Lower = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMin, InCellSize);
	const int64 Upper = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMaxExclusive - 1, InCellSize) + 1;
	const int64 MinX = FMath::Max(Lower, int64(InCoordinate.X) * InRegionSide);
	const int64 MinY = FMath::Max(Lower, int64(InCoordinate.Y) * InRegionSide);
	const int64 MaxX = FMath::Min(Upper, (int64(InCoordinate.X) + 1) * InRegionSide);
	const int64 MaxY = FMath::Min(Upper, (int64(InCoordinate.Y) + 1) * InRegionSide);
	if (MinX >= MaxX || MinY >= MaxY || Minimum != FIntPoint(int32(MinX), int32(MinY)) ||
		Width != MaxX - MinX || Height != MaxY - MinY || CellSize != InCellSize || Generation != InGeneration || !Fingerprint ||
		int64(ForestNeighbors.Num()) != int64(Width) * Height || CellLocations.Num() != ForestNeighbors.Num())
	{
		OutError = TEXT("Hydrology network requires the frozen nonoverlapping physical region core");
		return false;
	}
	OutError.Reset();
	return true;
}

FIntPoint FVoxelHydrologyRegionGraph::GetProfileCell(const int32 InIndex) const
{
	const uint32 Index = Profiles[InIndex].CellIndex;
	return Minimum + FIntPoint(Index % Width, Index / Width);
}

int32 FVoxelHydrologyRegionGraph::FindComponentAnchor(const FIntPoint& InCell) const
{
	const int64 LocalX = int64(InCell.X) - Minimum.X;
	const int64 LocalY = int64(InCell.Y) - Minimum.Y;
	if (LocalX < 0 || LocalY < 0 || LocalX >= Width || LocalY >= Height)
	{
		return INDEX_NONE;
	}
	const uint32 Index = uint32(LocalX + LocalY * Width);
	int32 Lower = 0;
	int32 Upper = AnchorRuns.Num();
	while (Lower < Upper)
	{
		const int32 Middle = Lower + (Upper - Lower) / 2;
		if (Index >= AnchorRuns[Middle].EndExclusive) Lower = Middle + 1;
		else Upper = Middle;
	}
	return AnchorRuns.IsValidIndex(Lower) ? AnchorRuns[Lower].Node : INDEX_NONE;
}

bool FVoxelHydrologyRegionGraph::ValidatePotentials(const int32 InEdge, const FVoxelHydrologyRegionPotential& InFirst,
	const FVoxelHydrologyRegionPotential& InSecond, FString& OutError) const
{
	if (!Edges.IsValidIndex(InEdge))
	{
		OutError = TEXT("Invalid hydrology region edge");
		return false;
	}
	const auto& Edge = Edges[InEdge];
	if (!Nodes.IsValidIndex(Edge.First) || !Nodes.IsValidIndex(Edge.Second) || Edge.ProfileOffset < 0 ||
		Edge.ProfileCount < 0 || int64(Edge.ProfileOffset) + Edge.ProfileCount > Profiles.Num() ||
		Edge.CellSteps != uint32(Edge.ProfileCount + 1))
	{
		OutError = TEXT("Invalid hydrology region edge profile range");
		return false;
	}
	const FVoxelHydrologyRegionPotential Potentials[] = {InFirst, InSecond};
	const int32 Endpoints[] = {Edge.First, Edge.Second};
	for (int32 Endpoint = 0; Endpoint < 2; ++Endpoint)
	{
		const auto& Node = Nodes[Endpoints[Endpoint]];
		const auto& Potential = Potentials[Endpoint];
		if (Potential.SpillZ == MAX_int32 || Potential.SpillZ < Node.GroundZ || Potential.PlateauDistance < 0 ||
			Potential.PlateauDistance > MAX_int64 - Edge.CellSteps ||
			(Node.IsReceiver() && (Potential.SpillZ != Node.WaterZ || Potential.PlateauDistance != 0)))
		{
			OutError = TEXT("Invalid or unresolved hydrology region endpoint potential");
			return false;
		}
		if (!Node.IsReceiver() && Potential.SpillZ > FMath::Max(Potentials[1 - Endpoint].SpillZ, Edge.BarrierZ))
		{
			OutError = TEXT("Hydrology region endpoint potential contradicts its connected path");
			return false;
		}
	}
	OutError.Reset();
	return true;
}

int32 FVoxelHydrologyRegionGraph::FindNodeIndex(const FIntPoint& InCell) const
{
	int32 Lower = 0;
	int32 Upper = Nodes.Num();
	while (Lower < Upper)
	{
		const int32 Middle = Lower + (Upper - Lower) / 2;
		const auto& Cell = Nodes[Middle].Cell;
		if (Cell.Y < InCell.Y || (Cell.Y == InCell.Y && Cell.X < InCell.X))
		{
			Lower = Middle + 1;
		}
		else
		{
			Upper = Middle;
		}
	}
	return Nodes.IsValidIndex(Lower) && Nodes[Lower].Cell == InCell ? Lower : INDEX_NONE;
}

EVoxelHydrologyRegionCellKind FVoxelHydrologyRegionGraph::FindCellLocation(const FIntPoint& InCell, int32& OutIndex) const
{
	OutIndex = INDEX_NONE;
	const int64 X = int64(InCell.X) - Minimum.X;
	const int64 Y = int64(InCell.Y) - Minimum.Y;
	if (X < 0 || Y < 0 || X >= Width || Y >= Height) return EVoxelHydrologyRegionCellKind::None;
	const uint32 Cell = uint32(X + Y * Width);
	if (!CellLocations.IsValidIndex(int32(Cell))) return EVoxelHydrologyRegionCellKind::None;
	const uint32 Location = CellLocations[Cell];
	OutIndex = int32(Location & ((1u << 30) - 1));
	switch (Location >> 30)
	{
	case 0: return Nodes.IsValidIndex(OutIndex) ? EVoxelHydrologyRegionCellKind::Node : EVoxelHydrologyRegionCellKind::None;
	case 1: return Profiles.IsValidIndex(OutIndex) ? EVoxelHydrologyRegionCellKind::Profile : EVoxelHydrologyRegionCellKind::None;
	case 2: return PrunedCells.IsValidIndex(OutIndex) ? EVoxelHydrologyRegionCellKind::Pruned : EVoxelHydrologyRegionCellKind::None;
	default:
		OutIndex = int32(Cell);
		return EVoxelHydrologyRegionCellKind::Wet;
	}
}

int32 FVoxelHydrologyRegionGraph::FindProfileEdge(const int32 InProfile) const
{
	if (!Profiles.IsValidIndex(InProfile)) return INDEX_NONE;
	int32 Lower = 0;
	int32 Upper = Edges.Num();
	while (Lower < Upper)
	{
		const int32 Middle = Lower + (Upper - Lower) / 2;
		const auto& Edge = Edges[Middle];
		if (int64(Edge.ProfileOffset) + Edge.ProfileCount <= InProfile) Lower = Middle + 1;
		else Upper = Middle;
	}
	return Edges.IsValidIndex(Lower) && Edges[Lower].ProfileOffset <= InProfile ? Lower : INDEX_NONE;
}

uint64 FVoxelHydrologyRegionGraph::GetMaxPrunedChildRainWeight(const uint32 InCellIndex) const
{
	int32 Lower = 0;
	int32 Upper = PrunedByParent.Num();
	while (Lower < Upper)
	{
		const int32 Middle = Lower + (Upper - Lower) / 2;
		if (PrunedCells[PrunedByParent[Middle]].Parent < InCellIndex) Lower = Middle + 1;
		else Upper = Middle;
	}
	uint64 Maximum = 0;
	while (Lower < PrunedByParent.Num())
	{
		const auto& Child = PrunedCells[PrunedByParent[Lower++]];
		if (Child.Parent != InCellIndex) break;
		Maximum = FMath::Max(Maximum, Child.RainWeight);
	}
	return Maximum;
}

uint32 FVoxelHydrologyRegionGraph::GetLastBarrierDistance(const int32 InEdge, const bool bFromFirst) const
{
	const auto& Edge = Edges[InEdge];
	const auto& Other = Nodes[bFromFirst ? Edge.Second : Edge.First];
	if ((Other.IsReceiver() ? Other.WaterZ : Other.GroundZ) == Edge.BarrierZ)
	{
		return Edge.CellSteps;
	}
	int32 Lower = 0;
	int32 Upper = Edge.ProfileCount;
	while (Lower < Upper)
	{
		const int32 Middle = Lower + (Upper - Lower) / 2;
		const auto& Profile = Profiles[Edge.ProfileOffset + Middle];
		if (bFromFirst ? Profile.SecondBarrierZ >= Edge.BarrierZ : Profile.FirstBarrierZ < Edge.BarrierZ)
		{
			Lower = Middle + 1;
		}
		else
		{
			Upper = Middle;
		}
	}
	return uint32(bFromFirst ? Lower : Edge.ProfileCount - Lower);
}

FVoxelHydrologyRegionRoute FVoxelHydrologyRegionGraph::ResolveProfileUnchecked(const int32 InEdge, const int32 InOffset,
	const FVoxelHydrologyRegionPotential& InFirst, const FVoxelHydrologyRegionPotential& InSecond) const
{
	const auto& Edge = Edges[InEdge];
	const auto& Profile = Profiles[Edge.ProfileOffset + InOffset];
	FVoxelHydrologyRegionPotential First;
	First.SpillZ = FMath::Max(InFirst.SpillZ, Profile.FirstBarrierZ);
	if (First.SpillZ == InFirst.SpillZ)
	{
		First.PlateauDistance = InFirst.PlateauDistance + InOffset + 1;
	}
	else
	{
		int32 Lower = 0;
		int32 Upper = InOffset;
		while (Lower < Upper)
		{
			const int32 Middle = Lower + (Upper - Lower) / 2;
			if (Profiles[Edge.ProfileOffset + Middle].FirstBarrierZ < First.SpillZ) Lower = Middle + 1;
			else Upper = Middle;
		}
		First.PlateauDistance = InOffset - Lower;
	}
	FVoxelHydrologyRegionPotential Second;
	Second.SpillZ = FMath::Max(InSecond.SpillZ, Profile.SecondBarrierZ);
	if (Second.SpillZ == InSecond.SpillZ)
	{
		Second.PlateauDistance = InSecond.PlateauDistance + Edge.ProfileCount - InOffset;
	}
	else
	{
		int32 Lower = InOffset;
		int32 Upper = Edge.ProfileCount - 1;
		while (Lower < Upper)
		{
			const int32 Middle = Lower + (Upper - Lower + 1) / 2;
			if (Profiles[Edge.ProfileOffset + Middle].SecondBarrierZ >= Second.SpillZ) Lower = Middle;
			else Upper = Middle - 1;
		}
		Second.PlateauDistance = Lower - InOffset;
	}
	const FIntPoint FirstParent = InOffset > 0 ? GetProfileCell(Edge.ProfileOffset + InOffset - 1) : Nodes[Edge.First].Cell;
	const FIntPoint SecondParent = InOffset + 1 < Edge.ProfileCount ? GetProfileCell(Edge.ProfileOffset + InOffset + 1) : Nodes[Edge.Second].Cell;
	const bool bTowardFirst = First.SpillZ < Second.SpillZ || (First.SpillZ == Second.SpillZ &&
		(First.PlateauDistance < Second.PlateauDistance || (First.PlateauDistance == Second.PlateauDistance &&
			(FirstParent.X < SecondParent.X || (FirstParent.X == SecondParent.X && FirstParent.Y < SecondParent.Y)))));
	return {bTowardFirst ? First : Second, bTowardFirst ? FirstParent : SecondParent, bTowardFirst};
}

bool FVoxelHydrologyRegionGraph::ResolveProfile(const int32 InEdge, const int32 InOffset,
	const FVoxelHydrologyRegionPotential& InFirst, const FVoxelHydrologyRegionPotential& InSecond,
	FVoxelHydrologyRegionRoute& OutRoute, FString& OutError) const
{
	if (!ValidatePotentials(InEdge, InFirst, InSecond, OutError))
	{
		return false;
	}
	if (InOffset < 0 || InOffset >= Edges[InEdge].ProfileCount)
	{
		OutError = TEXT("Invalid hydrology region interior offset");
		return false;
	}
	OutRoute = ResolveProfileUnchecked(InEdge, InOffset, InFirst, InSecond);
	return true;
}

bool FVoxelHydrologyRegionGraph::SplitEdgeRain(const int32 InEdge, const FVoxelHydrologyRegionPotential& InFirst,
	const FVoxelHydrologyRegionPotential& InSecond, FVoxelHydrologyRegionSplit& OutSplit, FString& OutError) const
{
	if (!ValidatePotentials(InEdge, InFirst, InSecond, OutError))
	{
		return false;
	}
	const auto& Edge = Edges[InEdge];
	int32 Lower = 0;
	int32 Upper = Edge.ProfileCount;
	while (Lower < Upper)
	{
		const int32 Middle = Lower + (Upper - Lower) / 2;
		if (ResolveProfileUnchecked(InEdge, Middle, InFirst, InSecond).bTowardFirst) Lower = Middle + 1;
		else Upper = Middle;
	}
	const uint64 FirstRain = Lower > 0 ? Profiles[Edge.ProfileOffset + Lower - 1].RainPrefix : 0;
	if (FirstRain > Edge.InteriorRainWeight)
	{
		OutError = TEXT("Invalid hydrology region interior rainfall prefix");
		return false;
	}
	OutSplit = {Lower, FirstRain, Edge.InteriorRainWeight - FirstRain};
	return true;
}

FVoxelHydrologyRegionGraphBuild::FVoxelHydrologyRegionGraphBuild()
{
}

FVoxelHydrologyRegionGraphBuild::~FVoxelHydrologyRegionGraphBuild()
{
}

bool FVoxelHydrologyRegionGraphBuild::Initialize(const FIntPoint& InMinimum, const int32 InWidth, const int32 InHeight, const int32 InCellSize, const uint64 InGeneration, FString& OutError)
{
	const int64 Count = int64(InWidth) * InHeight;
	if (Stage != EStage::None || InWidth <= 0 || InHeight <= 0 || InWidth > 4096 || InHeight > 4096 ||
		InCellSize <= 0 || !InGeneration || Count > MAX_int32 / 2 ||
		(int64(InMinimum.X) + 1) * InCellSize <= VoxelGeneration::WorldCellMin ||
		(int64(InMinimum.Y) + 1) * InCellSize <= VoxelGeneration::WorldCellMin ||
		(int64(InMinimum.X) + InWidth - 1) * InCellSize >= VoxelGeneration::WorldCellMaxExclusive ||
		(int64(InMinimum.Y) + InHeight - 1) * InCellSize >= VoxelGeneration::WorldCellMaxExclusive)
	{
		OutError = TEXT("Invalid hydrology region graph domain or generation");
		return false;
	}
	Result.Minimum = InMinimum;
	Result.Width = InWidth;
	Result.Height = InHeight;
	Result.CellSize = InCellSize;
	Result.Generation = InGeneration;
	Result.InputFingerprint = VoxelGeneration::Mix(InGeneration);
	CellCount = int32(Count);
	Stage = EStage::Allocate;
	OutError.Reset();
	return true;
}

bool FVoxelHydrologyRegionGraphBuild::Allocate(FString& OutError)
{
	if (!VoxelGenerationAllocation::RetainArray(Cells, CellCount, OutError) ||
		!VoxelGenerationAllocation::RetainArray(Candidates, CellCount * 2, OutError) ||
		!VoxelGenerationAllocation::RetainArray(Result.ForestNeighbors, CellCount, OutError) ||
		!VoxelGenerationAllocation::RetainArray(Result.CellLocations, CellCount, OutError))
	{
		return false;
	}
	return true;
}

FIntPoint FVoxelHydrologyRegionGraphBuild::Position(const int32 InIndex) const
{
	return Result.Minimum + FIntPoint(InIndex % Result.Width, InIndex / Result.Width);
}

uint8 FVoxelHydrologyRegionGraphBuild::BoundaryFaces(const int32 InIndex) const
{
	const int32 LocalX = InIndex % Result.Width;
	const int32 LocalY = InIndex / Result.Width;
	return (LocalX == 0 ? 1 : 0) | (LocalX + 1 == Result.Width ? 2 : 0) |
		(LocalY == 0 ? 4 : 0) | (LocalY + 1 == Result.Height ? 8 : 0);
}

bool FVoxelHydrologyRegionGraphBuild::IsTerminal(const int32 InIndex) const
{
	return BoundaryFaces(InIndex) || Cells[InIndex].Sample.IsReceiver() || Cells[InIndex].bAnchor;
}

bool FVoxelHydrologyRegionGraphBuild::KeepNode(const int32 InIndex) const
{
	const FCell& Cell = Cells[InIndex];
	return !Cell.bRemoved && (BoundaryFaces(InIndex) || Cell.Degree > 0 || !Cell.Sample.IsReceiver()) &&
		(IsTerminal(InIndex) || Cell.Degree != 2);
}

bool FVoxelHydrologyRegionGraphBuild::CandidateLess(const FCandidate& InLeft, const FCandidate& InRight) const
{
	if (InLeft.BarrierZ != InRight.BarrierZ) return InLeft.BarrierZ < InRight.BarrierZ;
	if (InLeft.First != InRight.First) return InLeft.First < InRight.First;
	return InLeft.Second < InRight.Second;
}

void FVoxelHydrologyRegionGraphBuild::Sift(int32 InRoot, const int32 InEnd)
{
	while (int64(InRoot) * 2 + 1 <= InEnd)
	{
		int32 Child = InRoot * 2 + 1;
		if (Child < InEnd && CandidateLess(Candidates[Child], Candidates[Child + 1])) ++Child;
		if (!CandidateLess(Candidates[InRoot], Candidates[Child])) break;
		Swap(Candidates[InRoot], Candidates[Child]);
		InRoot = Child;
	}
}

void FVoxelHydrologyRegionGraphBuild::SiftPruned(int32 InRoot, const int32 InEnd)
{
	const auto Less = [&](const int32 InFirst, const int32 InSecond)
	{
		const uint32 FirstParent = Result.PrunedCells[InFirst].Parent;
		const uint32 SecondParent = Result.PrunedCells[InSecond].Parent;
		return FirstParent < SecondParent || (FirstParent == SecondParent && InFirst < InSecond);
	};
	while (int64(InRoot) * 2 + 1 <= InEnd)
	{
		int32 Child = InRoot * 2 + 1;
		if (Child < InEnd && Less(Result.PrunedByParent[Child], Result.PrunedByParent[Child + 1])) ++Child;
		if (!Less(Result.PrunedByParent[InRoot], Result.PrunedByParent[Child])) break;
		Swap(Result.PrunedByParent[InRoot], Result.PrunedByParent[Child]);
		InRoot = Child;
	}
}

int32 FVoxelHydrologyRegionGraphBuild::FindRoot(int32 InIndex)
{
	while (Cells[InIndex].Parent != InIndex)
	{
		Cells[InIndex].Parent = Cells[Cells[InIndex].Parent].Parent;
		InIndex = Cells[InIndex].Parent;
	}
	return InIndex;
}

void FVoxelHydrologyRegionGraphBuild::QueueLeaf(const int32 InIndex)
{
	FCell& Cell = Cells[InIndex];
	if (!IsTerminal(InIndex) && Cell.Degree <= 1 && !Cell.bQueued)
	{
		Cell.bQueued = true;
		Leaves.Add(InIndex);
	}
}

void FVoxelHydrologyRegionGraphBuild::FreeScratch()
{
	VoxelGenerationAllocation::FreeRetainedArray(Cells);
	VoxelGenerationAllocation::FreeRetainedArray(Candidates);
	VoxelGenerationAllocation::FreeRetainedArray(Links);
	VoxelGenerationAllocation::FreeRetainedArray(Leaves);
}

void FVoxelHydrologyRegionGraphBuild::AddFingerprint(const uint64 InValue)
{
	Result.Fingerprint = VoxelGeneration::Mix(Result.Fingerprint ^ InValue);
}

EVoxelHydrologyRegionStep FVoxelHydrologyRegionGraphBuild::Failure(FString& OutError) const
{
	if (OutError == TEXT("Canceled")) return EVoxelHydrologyRegionStep::Canceled;
	if (OutError.StartsWith(TEXT("CapacityBlocked"))) return EVoxelHydrologyRegionStep::CapacityBlocked;
	if (OutError.IsEmpty()) OutError = TEXT("Invalid hydrology region graph build");
	return EVoxelHydrologyRegionStep::Failed;
}

EVoxelHydrologyRegionStep FVoxelHydrologyRegionGraphBuild::Advance(const uint64 InGeneration, FReadCell InReadCell, int32 InWorkBudget, FString& OutError, const TAtomic<bool>* InCancel)
{
	static TAtomic<uint64> NextDiagnosticMilliseconds{0};
	const uint64 Now = uint64(FPlatformTime::Seconds() * 1000.0);
	uint64 Expected = NextDiagnosticMilliseconds.Load();
	if (Now >= Expected && NextDiagnosticMilliseconds.CompareExchange(Expected, Now + 5000))
	{
		UE_LOG(LogTemp, Display, TEXT("Voxel macro graph progress: generation=%llu min=(%d,%d) stage=%d cursor=%d sortEnd=%d cells=%d nodes=%d working=%llu"),
			Result.Generation, Result.Minimum.X, Result.Minimum.Y, int32(Stage), Cursor, SortEnd, CellCount, Result.Nodes.Num(), GetAllocatedBytes());
	}
	OutError.Reset();
	if (Stage == EStage::None || Stage == EStage::Taken || InGeneration != Result.Generation)
	{
		OutError = TEXT("Invalid hydrology region graph generation or lifetime");
		return Failure(OutError);
	}
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return EVoxelHydrologyRegionStep::Canceled;
	}
	while (InWorkBudget-- > 0)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return EVoxelHydrologyRegionStep::Canceled;
		}
		if (Stage == EStage::Allocate)
		{
			Stage = EStage::Read;
		}
		else if (Stage == EStage::Read)
		{
			if (Cursor == CellCount)
			{
				SortRoot = Candidates.Num() / 2 - 1;
				SortEnd = Candidates.Num() - 1;
				Cursor = 0;
				Stage = EStage::Sort;
				continue;
			}
			FVoxelHydrologyRegionSample Sample;
			const auto Read = InReadCell(Position(Cursor), Sample, OutError);
			if (Read == EVoxelHydrologyClosureRead::Pending) return EVoxelHydrologyRegionStep::NeedInput;
			if (Read != EVoxelHydrologyClosureRead::Ready) return Failure(OutError);
			if (!Sample.IsValid())
			{
				OutError = TEXT("Invalid hydrology region ground, rainfall or positive identified receiver");
				return Failure(OutError);
			}
			if (Cursor == 0 && !Allocate(OutError)) return Failure(OutError);
			FCell& Cell = Cells.AddDefaulted_GetRef();
			Cell.Sample = Sample;
			Cell.Parent = Cursor;
			Cell.RainWeight = Sample.IsReceiver() ? 0 : Sample.RainWeight;
			Result.TotalLandRainWeight += Cell.RainWeight;
			Result.InputFingerprint = Sample.AppendFingerprint(Result.InputFingerprint);
			Result.ForestNeighbors.Add(0);
			Result.CellLocations.Add(3u << 30);
			// 左/上输入已冻结，读格时直接形成同一无向边；排序键仍为原 First/Second。
			const int32 Neighbors[] = {Cursor % Result.Width > 0 ? Cursor - 1 : INDEX_NONE,
				Cursor >= Result.Width ? Cursor - Result.Width : INDEX_NONE};
			for (const int32 Neighbor : Neighbors)
			{
				if (Neighbor == INDEX_NONE) continue;
				const auto& First = Cells[Neighbor].Sample;
				if (First.IsReceiver() && Sample.IsReceiver()) continue;
				Candidates.Add({Neighbor, Cursor, FMath::Max(First.IsReceiver() ? First.WaterZ : First.GroundZ,
					Sample.IsReceiver() ? Sample.WaterZ : Sample.GroundZ), false});
			}
			++Cursor;
		}
		else if (Stage == EStage::Sort)
		{
			if (SortRoot >= 0) Sift(SortRoot--, SortEnd);
			else if (SortEnd > 0) { Swap(Candidates[0], Candidates[SortEnd]); Sift(0, --SortEnd); }
			else { Cursor = 0; Stage = EStage::Forest; }
		}
		else if (Stage == EStage::Forest)
		{
			if (Cursor == Candidates.Num())
			{
				// 稳定压实已选边，保留原 Kruskal 顺序和邻接边顺序，不增加任何 backing。
				Candidates.SetNum(KeptEdges, EAllowShrinking::No);
				// 授额等待后仍停在此阶段，游标必须对应压实后的正文长度。
				Cursor = KeptEdges;
				if (!VoxelGenerationAllocation::RetainArray(Links, KeptEdges * 2, OutError)) return Failure(OutError);
				Cursor = 0;
				Stage = EStage::Links;
				continue;
			}
			FCandidate Edge = Candidates[Cursor++];
			if (Cells[Edge.First].Sample.IsReceiver() || Cells[Edge.Second].Sample.IsReceiver()) Edge.bKept = true;
			else
			{
				int32 FirstRoot = FindRoot(Edge.First);
				int32 SecondRoot = FindRoot(Edge.Second);
				if (FirstRoot != SecondRoot)
				{
					if (Cells[FirstRoot].Rank < Cells[SecondRoot].Rank ||
						(Cells[FirstRoot].Rank == Cells[SecondRoot].Rank && FirstRoot > SecondRoot)) Swap(FirstRoot, SecondRoot);
					Cells[SecondRoot].Parent = FirstRoot;
					if (Cells[FirstRoot].Rank == Cells[SecondRoot].Rank) ++Cells[FirstRoot].Rank;
					Edge.bKept = true;
				}
			}
			if (Edge.bKept) Candidates[KeptEdges++] = Edge;
		}
		else if (Stage == EStage::Links)
		{
			if (Cursor == Candidates.Num())
			{
				VoxelGenerationAllocation::FreeRetainedArray(Candidates);
				Cursor = 0;
				Stage = EStage::Leaves;
				continue;
			}
			const int32 EdgeIndex = Cursor++;
			const auto& Edge = Candidates[EdgeIndex];
			if (!Edge.bKept) continue;
			Cells[Edge.First].Head = Links.Add({Edge.Second, Edge.BarrierZ, Cells[Edge.First].Head});
			Cells[Edge.Second].Head = Links.Add({Edge.First, Edge.BarrierZ, Cells[Edge.Second].Head});
			++Cells[Edge.First].Degree;
			++Cells[Edge.Second].Degree;
			if (Edge.Second == Edge.First + 1 && Edge.First % Result.Width + 1 < Result.Width)
			{
				Result.ForestNeighbors[Edge.First] |= 2;
				Result.ForestNeighbors[Edge.Second] |= 1;
			}
			else
			{
				check(Edge.Second == Edge.First + Result.Width);
				Result.ForestNeighbors[Edge.First] |= 8;
				Result.ForestNeighbors[Edge.Second] |= 4;
			}
		}
		else if (Stage == EStage::Leaves)
		{
			if (Cursor == 0 && !VoxelGenerationAllocation::RetainArray(Leaves, CellCount, OutError)) return Failure(OutError);
			if (Cursor == CellCount) { Cursor = 0; Stage = EStage::Peel; continue; }
			Cells[Cursor].bAnchor = !Cells[Cursor].Sample.IsReceiver() && FindRoot(Cursor) == Cursor;
			QueueLeaf(Cursor++);
		}
		else if (Stage == EStage::Peel)
		{
			if (Cursor == Leaves.Num())
			{
				VoxelGenerationAllocation::FreeRetainedArray(Leaves);
				Cursor = 0;
				Stage = EStage::CountNodes;
				continue;
			}
			const int32 Leaf = Leaves[Cursor];
			FCell& Cell = Cells[Leaf];
			if (Cell.bRemoved || IsTerminal(Leaf)) { ++Cursor; continue; }
			for (int32 Link = Cell.Head; Link != INDEX_NONE; Link = Links[Link].Next)
			{
				FCell& Neighbor = Cells[Links[Link].Neighbor];
				if (Neighbor.bRemoved) continue;
				const int32 Required = Result.PrunedCells.Num() + 1;
				const int32 Capacity = FMath::Max(4, Result.PrunedCells.Num() * 2);
				if ((Required > Result.PrunedCells.Max() && !VoxelGenerationAllocation::RetainArray(Result.PrunedCells, Capacity, OutError)) ||
					(Required > Result.PrunedByParent.Max() && !VoxelGenerationAllocation::RetainArray(Result.PrunedByParent, Capacity, OutError))) return Failure(OutError);
				const int32 Pruned = Result.PrunedCells.Add({uint32(Links[Link].Neighbor), uint32(Leaf), Cell.RainWeight, Cell.Sample.GroundZ});
				Result.PrunedByParent.Add(Pruned);
				Result.CellLocations[Leaf] = (2u << 30) | uint32(Pruned);
				Neighbor.RainWeight += Cell.RainWeight;
				Cell.RainWeight = 0;
				--Neighbor.Degree;
				QueueLeaf(Links[Link].Neighbor);
				Cell.bRemoved = true;
				break;
			}
			++Cursor;
		}
		else if (Stage == EStage::CountNodes)
		{
			if (Cursor < CellCount)
			{
				if (KeepNode(Cursor))
				{
					++NodeCount;
					SummaryHalfEdges += Cells[Cursor].Degree;
				}
				else if (!Cells[Cursor].bRemoved && Cells[Cursor].Degree == 2) ++InteriorCells;
				// 根格索引与稍后发布的节点索引一一对应，RLE 切分数可在此同步精确统计。
				const int32 Anchor = Cells[Cursor].Sample.IsReceiver() ? INDEX_NONE : FindRoot(Cursor);
				if (Anchor != PreviousAnchor) { ++AnchorCount; PreviousAnchor = Anchor; }
				++Cursor;
				continue;
			}
			check(SummaryHalfEdges % 2 == 0);
			if (!VoxelGenerationAllocation::RetainArray(Result.Nodes, NodeCount, OutError) ||
				!VoxelGenerationAllocation::RetainArray(Result.Edges, SummaryHalfEdges / 2, OutError) ||
				!VoxelGenerationAllocation::RetainArray(Result.Profiles, InteriorCells, OutError) ||
				!VoxelGenerationAllocation::RetainArray(Result.AnchorRuns, AnchorCount, OutError)) return Failure(OutError);
			Cursor = 0;
			Stage = EStage::Nodes;
		}
		else if (Stage == EStage::Nodes)
		{
			if (Cursor == CellCount) { Cursor = 0; Stage = EStage::CellAnchors; continue; }
			if (KeepNode(Cursor))
			{
				FCell& Cell = Cells[Cursor];
				Cell.Node = Result.Nodes.Add({Position(Cursor), Cell.Sample.GroundZ, Cell.Sample.WaterZ,
					Cell.Sample.WaterId, Cell.RainWeight, BoundaryFaces(Cursor)});
				Result.CellLocations[Cursor] = uint32(Cell.Node);
			}
			++Cursor;
		}
		else if (Stage == EStage::CellAnchors)
		{
			if (Cursor == CellCount)
			{
				Cursor = 0;
				Stage = EStage::Edges;
				continue;
			}
			const int32 Anchor = Cells[Cursor].Sample.IsReceiver() ? INDEX_NONE : Cells[FindRoot(Cursor)].Node;
			if (Result.AnchorRuns.IsEmpty() || Result.AnchorRuns.Last().Node != Anchor)
			{
				Result.AnchorRuns.Add({uint32(Cursor + 1), Anchor});
			}
			else Result.AnchorRuns.Last().EndExclusive = uint32(Cursor + 1);
			++Cursor;
		}
		else if (Stage == EStage::Edges)
		{
			if (TraceEdge != INDEX_NONE)
			{
				const auto& Edge = Result.Edges[TraceEdge];
				if (TraceCurrent == TraceStop)
				{
					check(Result.Profiles.Num() - Edge.ProfileOffset == Edge.ProfileCount && TraceRainWeight == Edge.InteriorRainWeight);
					TraceEdge = INDEX_NONE;
					continue;
				}
				const FCell& Cell = Cells[TraceCurrent];
				TraceBarrierZ = FMath::Max(TraceBarrierZ, Cell.Sample.GroundZ);
				TraceRainWeight += Cell.RainWeight;
				Result.CellLocations[TraceCurrent] = (1u << 30) | uint32(Result.Profiles.Num());
				const int32 FirstMinimum = FMath::Min(Cell.Sample.GroundZ,
					Result.Profiles.Num() > Edge.ProfileOffset ? Result.Profiles.Last().FirstMinimumGroundZ : MAX_int32);
				Result.Profiles.Add({uint32(TraceCurrent), Cell.Sample.GroundZ, TraceBarrierZ, MIN_int32, TraceRainWeight, FirstMinimum});
				int32 NextLink = Cell.Head;
				while (NextLink != INDEX_NONE && (Cells[Links[NextLink].Neighbor].bRemoved ||
					Links[NextLink].Neighbor == TracePrevious)) NextLink = Links[NextLink].Next;
				check(NextLink != INDEX_NONE);
				TracePrevious = TraceCurrent;
				TraceCurrent = Links[NextLink].Neighbor;
				continue;
			}
			if (WalkCurrent != INDEX_NONE)
			{
				const FCell& Cell = Cells[WalkCurrent];
				if (Cell.Node != INDEX_NONE)
				{
					if (WalkStart < WalkCurrent || (WalkStart == WalkCurrent && WalkFirst < WalkPrevious))
					{
						TraceEdge = Result.Edges.Add({Cells[WalkStart].Node, Cell.Node, WalkBarrierZ, WalkSteps,
							WalkRainWeight, Result.Profiles.Num(), int32(WalkSteps - 1), Position(WalkFirst), Position(WalkPrevious)});
						for (const int32 Endpoint : {Cells[WalkStart].Node, Cell.Node})
						{
							auto& Node = Result.Nodes[Endpoint];
							check(Node.EdgeCount < 4);
							Node.Edges[Node.EdgeCount++] = TraceEdge;
						}
						TracePrevious = WalkStart;
						TraceCurrent = WalkFirst;
						TraceStop = WalkCurrent;
						const auto& Start = Cells[WalkStart].Sample;
						TraceBarrierZ = Start.IsReceiver() ? Start.WaterZ : Start.GroundZ;
						TraceRainWeight = 0;
					}
					WalkCurrent = INDEX_NONE;
					continue;
				}
				WalkRainWeight += Cell.RainWeight;
				int32 NextLink = Cell.Head;
				while (NextLink != INDEX_NONE && (Cells[Links[NextLink].Neighbor].bRemoved ||
					Links[NextLink].Neighbor == WalkPrevious)) NextLink = Links[NextLink].Next;
				check(NextLink != INDEX_NONE);
				WalkBarrierZ = FMath::Max(WalkBarrierZ, Links[NextLink].BarrierZ);
				WalkPrevious = WalkCurrent;
				WalkCurrent = Links[NextLink].Neighbor;
				++WalkSteps;
				continue;
			}
			if (LinkCursor != INDEX_NONE)
			{
				const FLink& Link = Links[LinkCursor];
				LinkCursor = Link.Next;
				if (Cells[Link.Neighbor].bRemoved) continue;
				const FIntPoint Local = Result.Nodes[Cursor - 1].Cell - Result.Minimum;
				WalkStart = Local.X + Local.Y * Result.Width;
				WalkPrevious = WalkStart;
				WalkCurrent = WalkFirst = Link.Neighbor;
				WalkBarrierZ = Link.BarrierZ;
				WalkSteps = 1;
				WalkRainWeight = 0;
				continue;
			}
			if (Cursor < Result.Nodes.Num())
			{
				const FIntPoint Local = Result.Nodes[Cursor].Cell - Result.Minimum;
				LinkCursor = Cells[Local.X + Local.Y * Result.Width].Head;
				++Cursor;
				continue;
			}
			VoxelGenerationAllocation::FreeRetainedArray(Links);
			Cursor = 0;
			Stage = EStage::Profiles;
		}
		else if (Stage == EStage::Profiles)
		{
			if (Cursor < Result.Edges.Num())
			{
				const auto& Edge = Result.Edges[Cursor];
				if (!bSuffixActive)
				{
					SuffixCursor = Edge.ProfileOffset + Edge.ProfileCount;
					const auto& End = Result.Nodes[Edge.Second];
					SuffixBarrierZ = End.IsReceiver() ? End.WaterZ : End.GroundZ;
					bSuffixActive = true;
				}
				if (SuffixCursor > Edge.ProfileOffset)
				{
					auto& Profile = Result.Profiles[--SuffixCursor];
					SuffixBarrierZ = FMath::Max(SuffixBarrierZ, Profile.GroundZ);
					Profile.SecondBarrierZ = SuffixBarrierZ;
					Profile.SecondMinimumGroundZ = FMath::Min(Profile.GroundZ,
						SuffixCursor + 1 < Edge.ProfileOffset + Edge.ProfileCount
						? Result.Profiles[SuffixCursor + 1].SecondMinimumGroundZ : MAX_int32);
					continue;
				}
				bSuffixActive = false;
				++Cursor;
				continue;
			}
			Result.Fingerprint = VoxelGeneration::Mix(Result.InputFingerprint ^ Result.Generation);
			AddFingerprint(uint32(Result.Minimum.X));
			AddFingerprint(uint32(Result.Minimum.Y));
			AddFingerprint(Result.Width);
			AddFingerprint(Result.Height);
			AddFingerprint(Result.CellSize);
			Cursor = 0;
			Stage = EStage::CheckNodes;
		}
		else if (Stage == EStage::CheckNodes)
		{
			if (Cursor == Result.Nodes.Num()) { Cursor = 0; TraceEdge = 0; Stage = EStage::CheckProfiles; continue; }
			const auto& Node = Result.Nodes[Cursor++];
			const FIntPoint Local = Node.Cell - Result.Minimum;
			check(Node.EdgeCount == Cells[Local.X + Local.Y * Result.Width].Degree);
			for (int32 Slot = 0; Slot < Node.EdgeCount; ++Slot)
			{
				const auto& Edge = Result.Edges[Node.Edges[Slot]];
				check(Edge.First == Cursor - 1 || Edge.Second == Cursor - 1);
				AddFingerprint(uint32(Node.Edges[Slot]));
			}
			CheckedRainWeight += Node.RainWeight;
			AddFingerprint(uint32(Node.Cell.X));
			AddFingerprint(uint32(Node.Cell.Y));
			AddFingerprint(Node.RainWeight);
			AddFingerprint(Node.BoundaryFaces);
			AddFingerprint(uint32(Node.GroundZ));
			AddFingerprint(uint32(Node.WaterZ));
			AddFingerprint(Node.WaterId.High);
			AddFingerprint(Node.WaterId.Low);
		}
		else if (Stage == EStage::CheckProfiles)
		{
			if (Cursor == Result.Profiles.Num()) { Cursor = 0; Stage = EStage::CheckEdges; continue; }
			check(Result.Edges.IsValidIndex(TraceEdge));
			const auto& Edge = Result.Edges[TraceEdge];
			if (Cursor == Edge.ProfileOffset + Edge.ProfileCount) { ++TraceEdge; continue; }
			auto& Profile = Result.Profiles[Cursor];
			check(Profile.CellIndex < uint32(Cells.Num()));
			FCell& Cell = Cells[Profile.CellIndex];
			const uint64 PreviousRain = Cursor == Edge.ProfileOffset ? 0 : Result.Profiles[Cursor - 1].RainPrefix;
			if (Cell.bProfiled || Cell.bRemoved || Cell.Node != INDEX_NONE || Profile.GroundZ != Cell.Sample.GroundZ ||
				Profile.RainPrefix < PreviousRain || Profile.RainPrefix - PreviousRain != Cell.RainWeight)
			{
				OutError = TEXT("Invalid hydrology region interior profile or rainfall ownership");
				return Failure(OutError);
			}
			Cell.bProfiled = true;
			AddFingerprint(Profile.CellIndex);
			AddFingerprint(uint32(Profile.GroundZ));
			AddFingerprint(uint32(Profile.FirstBarrierZ));
			AddFingerprint(uint32(Profile.SecondBarrierZ));
			AddFingerprint(Profile.RainPrefix);
			AddFingerprint(uint32(Profile.FirstMinimumGroundZ));
			AddFingerprint(uint32(Profile.SecondMinimumGroundZ));
			++Cursor;
		}
		else if (Stage == EStage::CheckEdges)
		{
			if (Cursor < Result.Edges.Num())
			{
				const auto& Edge = Result.Edges[Cursor++];
				check(Edge.CellSteps == uint32(Edge.ProfileCount + 1));
				check(Edge.ProfileCount == 0 || Result.Profiles[Edge.ProfileOffset + Edge.ProfileCount - 1].RainPrefix == Edge.InteriorRainWeight);
				CheckedRainWeight += Edge.InteriorRainWeight;
				AddFingerprint(Edge.First);
				AddFingerprint(Edge.Second);
				AddFingerprint(uint32(Edge.BarrierZ));
				AddFingerprint(Edge.CellSteps);
				AddFingerprint(Edge.InteriorRainWeight);
				AddFingerprint(Edge.ProfileOffset);
				AddFingerprint(Edge.ProfileCount);
				AddFingerprint(uint32(Edge.FirstStep.X));
				AddFingerprint(uint32(Edge.FirstStep.Y));
				AddFingerprint(uint32(Edge.LastStep.X));
				AddFingerprint(uint32(Edge.LastStep.Y));
				continue;
			}
			if (CheckedRainWeight != Result.TotalLandRainWeight || Result.Edges.Num() != SummaryHalfEdges / 2 || Result.Profiles.Num() != InteriorCells)
			{
				OutError = TEXT("Hydrology region summary does not conserve land rainfall");
				return Failure(OutError);
			}
			if (!Result.Fingerprint) Result.Fingerprint = 1;
			Cursor = 0;
			Stage = EStage::CheckAnchors;
		}
		else if (Stage == EStage::CheckAnchors)
		{
			if (Cursor < CellCount)
			{
				const auto& Cell = Cells[Cursor];
				const int32 Anchor = Result.FindComponentAnchor(Position(Cursor));
				if ((!Cell.Sample.IsReceiver() && (!Result.Nodes.IsValidIndex(Anchor) || Result.Nodes[Anchor].IsReceiver())) ||
					(Cell.Sample.IsReceiver() && Anchor != INDEX_NONE))
				{
					OutError = TEXT("Invalid hydrology region dry component anchor");
					return Failure(OutError);
				}
				AddFingerprint(uint32(Anchor));
				AddFingerprint(Result.ForestNeighbors[Cursor]);
				AddFingerprint(Result.CellLocations[Cursor]);
				++Cursor;
				continue;
			}
			if (Result.AnchorRuns.Num() != AnchorCount || Result.AnchorRuns.IsEmpty() || Result.AnchorRuns.Last().EndExclusive != uint32(CellCount))
			{
				OutError = TEXT("Invalid hydrology region component coverage");
				return Failure(OutError);
			}
			VoxelGenerationAllocation::FreeRetainedArray(Cells);
			SortRoot = Result.PrunedByParent.Num() / 2 - 1;
			SortEnd = Result.PrunedByParent.Num() - 1;
			Stage = EStage::SortPruned;
		}
		else if (Stage == EStage::SortPruned)
		{
			if (SortRoot >= 0) SiftPruned(SortRoot--, SortEnd);
			else if (SortEnd > 0) { Swap(Result.PrunedByParent[0], Result.PrunedByParent[SortEnd]); SiftPruned(0, --SortEnd); }
			else { Cursor = 0; Stage = EStage::CheckPruned; }
		}
		else if (Stage == EStage::CheckPruned)
		{
			if (Cursor < Result.PrunedCells.Num())
			{
				const auto& Pruned = Result.PrunedCells[Cursor];
				if (Pruned.Parent >= uint32(CellCount) || Pruned.CellIndex >= uint32(CellCount) ||
					Result.CellLocations[Pruned.CellIndex] != ((2u << 30) | uint32(Cursor)))
				{
					OutError = TEXT("Invalid hydrology pruned branch source facts");
					return Failure(OutError);
				}
				AddFingerprint(Pruned.Parent);
				AddFingerprint(Pruned.CellIndex);
				AddFingerprint(Pruned.RainWeight);
				AddFingerprint(uint32(Pruned.GroundZ));
				AddFingerprint(uint32(Result.PrunedByParent[Cursor++]));
				continue;
			}
			if (!Result.Fingerprint) Result.Fingerprint = 1;
			FreeScratch();
			Stage = EStage::Ready;
		}
		else if (Stage == EStage::Ready) return EVoxelHydrologyRegionStep::Ready;
	}
	return Stage == EStage::Ready ? EVoxelHydrologyRegionStep::Ready : EVoxelHydrologyRegionStep::Yielded;
}

bool FVoxelHydrologyRegionGraphBuild::IsReady() const
{
	return Stage == EStage::Ready;
}

const FVoxelHydrologyRegionGraph& FVoxelHydrologyRegionGraphBuild::GetResult() const
{
	check(IsReady());
	return Result;
}

FVoxelHydrologyRegionGraph FVoxelHydrologyRegionGraphBuild::TakeResult()
{
	check(IsReady());
	Stage = EStage::Taken;
	return MoveTemp(Result);
}

uint64 FVoxelHydrologyRegionGraphBuild::GetAllocatedBytes() const
{
	return Result.GetAllocatedBytes() + VoxelGenerationAllocation::AllocatedBytes(Cells) +
		VoxelGenerationAllocation::AllocatedBytes(Candidates) + VoxelGenerationAllocation::AllocatedBytes(Links) +
		VoxelGenerationAllocation::AllocatedBytes(Leaves);
}
