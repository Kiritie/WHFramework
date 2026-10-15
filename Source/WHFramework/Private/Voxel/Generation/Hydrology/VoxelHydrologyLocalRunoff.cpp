#include "Voxel/Generation/Hydrology/VoxelHydrologyLocalRunoff.h"

#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationMath.h"

namespace
{
	bool HydrologyLocalCellLess(const FIntPoint& InFirst, const FIntPoint& InSecond)
	{
		return InFirst.X < InSecond.X || (InFirst.X == InSecond.X && InFirst.Y < InSecond.Y);
	}
}

const FVoxelHydrologyLocalCell* FVoxelHydrologyLocalRunoff::Find(const FIntPoint& InCell) const
{
	const int64 X = int64(InCell.X) - Minimum.X;
	const int64 Y = int64(InCell.Y) - Minimum.Y;
	return X >= 0 && Y >= 0 && X < Width && Y < Height ? &Cells[int32(X + Y * Width)] : nullptr;
}

uint64 FVoxelHydrologyLocalRunoff::GetAllocatedBytes() const
{
	return VoxelGenerationAllocation::AllocatedBytes(Cells);
}

FVoxelHydrologyLocalRunoffBuild::FVoxelHydrologyLocalRunoffBuild()
{
}

FVoxelHydrologyLocalRunoffBuild::~FVoxelHydrologyLocalRunoffBuild()
{
}

bool FVoxelHydrologyLocalRunoffBuild::Initialize(const FVoxelHydrologyRegionGraph& InGraph, FString& OutError)
{
	const int64 Count = int64(InGraph.Width) * InGraph.Height;
	if (Stage != EStage::None || !InGraph.Fingerprint || !InGraph.Generation || InGraph.CellSize <= 0 ||
		InGraph.Width <= 0 || InGraph.Height <= 0 || InGraph.Width > 4096 || InGraph.Height > 4096 ||
		Count > MAX_int32 / 2 || InGraph.ForestNeighbors.Num() != Count ||
		(int64(InGraph.Minimum.X) + 1) * InGraph.CellSize <= VoxelGeneration::WorldCellMin ||
		(int64(InGraph.Minimum.Y) + 1) * InGraph.CellSize <= VoxelGeneration::WorldCellMin ||
		(int64(InGraph.Minimum.X) + InGraph.Width - 1) * InGraph.CellSize >= VoxelGeneration::WorldCellMaxExclusive ||
		(int64(InGraph.Minimum.Y) + InGraph.Height - 1) * InGraph.CellSize >= VoxelGeneration::WorldCellMaxExclusive)
	{
		OutError = TEXT("Invalid hydrology local runoff lifetime or unpublished graph");
		return false;
	}
	Result.Minimum = InGraph.Minimum;
	Result.Width = InGraph.Width;
	Result.Height = InGraph.Height;
	Result.CellSize = InGraph.CellSize;
	Result.Generation = InGraph.Generation;
	Result.GraphFingerprint = InGraph.Fingerprint;
	InputFingerprint = InGraph.InputFingerprint;
	CapturedInputFingerprint = VoxelGeneration::Mix(InGraph.Generation);
	CellCount = InGraph.Width * InGraph.Height;
	Stage = EStage::Allocate;
	OutError.Reset();
	return true;
}

FIntPoint FVoxelHydrologyLocalRunoffBuild::Position(const int32 InIndex) const
{
	return Result.Minimum + FIntPoint(InIndex % Result.Width, InIndex / Result.Width);
}

int32 FVoxelHydrologyLocalRunoffBuild::IndexOf(const FIntPoint& InCell) const
{
	const int64 X = int64(InCell.X) - Result.Minimum.X;
	const int64 Y = int64(InCell.Y) - Result.Minimum.Y;
	return X >= 0 && Y >= 0 && X < Result.Width && Y < Result.Height ? int32(X + Y * Result.Width) : INDEX_NONE;
}

bool FVoxelHydrologyLocalRunoffBuild::IsBoundary(const int32 InIndex) const
{
	return InIndex % Result.Width == 0 || InIndex % Result.Width + 1 == Result.Width ||
		InIndex < Result.Width || InIndex >= CellCount - Result.Width;
}

bool FVoxelHydrologyLocalRunoffBuild::EventLess(const FEvent& InFirst, const FEvent& InSecond) const
{
	if (InFirst.Plane != InSecond.Plane) return InFirst.Plane < InSecond.Plane;
	if (InFirst.Distance != InSecond.Distance) return InFirst.Distance < InSecond.Distance;
	return HydrologyLocalCellLess(Position(InFirst.Index), Position(InSecond.Index));
}

bool FVoxelHydrologyLocalRunoffBuild::Push(const FEvent& InEvent, FString& OutError)
{
	if (Frontier.Num() == MAX_int32)
	{
		OutError = TEXT("Oversized: hydrology local runoff frontier");
		return false;
	}
	if (Frontier.Num() == Frontier.Max())
	{
		const int32 Capacity = int32(FMath::Min<int64>(MAX_int32, FMath::Max<int64>(4, int64(Frontier.Num()) * 2)));
		if (!VoxelGenerationAllocation::RetainArray(Frontier, Capacity, OutError)) return false;
	}
	int32 Index = Frontier.Add(InEvent);
	while (Index > 0)
	{
		const int32 Parent = (Index - 1) / 2;
		if (!EventLess(Frontier[Index], Frontier[Parent])) break;
		Swap(Frontier[Index], Frontier[Parent]);
		Index = Parent;
	}
	return true;
}

FVoxelHydrologyLocalRunoffBuild::FEvent FVoxelHydrologyLocalRunoffBuild::Pop()
{
	const FEvent Event = Frontier[0];
	const FEvent Last = Frontier.Pop(EAllowShrinking::No);
	if (!Frontier.IsEmpty())
	{
		Frontier[0] = Last;
		int32 Index = 0;
		while (int64(Index) * 2 + 1 < Frontier.Num())
		{
			int32 Child = Index * 2 + 1;
			if (Child + 1 < Frontier.Num() && EventLess(Frontier[Child + 1], Frontier[Child])) ++Child;
			if (!EventLess(Frontier[Child], Frontier[Index])) break;
			Swap(Frontier[Child], Frontier[Index]);
			Index = Child;
		}
	}
	return Event;
}

EVoxelHydrologyRegionStep FVoxelHydrologyLocalRunoffBuild::Failure(FString& OutError) const
{
	if (OutError == TEXT("Canceled")) return EVoxelHydrologyRegionStep::Canceled;
	if (OutError.StartsWith(TEXT("CapacityBlocked"))) return EVoxelHydrologyRegionStep::CapacityBlocked;
	if (OutError.IsEmpty()) OutError = TEXT("HydrologyComponentConflict: invalid local runoff");
	return EVoxelHydrologyRegionStep::Failed;
}

EVoxelHydrologyRegionStep FVoxelHydrologyLocalRunoffBuild::Advance(const FVoxelHydrologyRegionGraph& InGraph,
	const FVoxelHydrologyNetworkRunoff& InRunoff, FVoxelHydrologyRegionGraphBuild::FReadCell InReadCell,
	const int32 InWorkBudget, FString& OutError, const TAtomic<bool>* InCancel)
{
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return EVoxelHydrologyRegionStep::Canceled;
	}
	if (!InRunoff.Fingerprint || InRunoff.Generation != InGraph.Generation || InRunoff.GraphFingerprint != InGraph.Fingerprint)
	{
		OutError = TEXT("HydrologyComponentConflict: local runoff requires completed matching macro inflow");
		return EVoxelHydrologyRegionStep::Failed;
	}
	const auto ReadBoundary = [&](const FIntPoint& InCell, FVoxelHydrologyNetworkCell& OutControl,
		uint64& OutIncomingRainWeight, FString& InError)
	{
		const auto* Boundary = InRunoff.Find(InCell);
		if (!Boundary)
		{
			InError = TEXT("HydrologyComponentConflict: published macro inflow omits a local boundary");
			return EVoxelHydrologyClosureRead::Failed;
		}
		OutControl = Boundary->Control;
		OutIncomingRainWeight = Boundary->IncomingRainWeight;
		InError.Reset();
		return EVoxelHydrologyClosureRead::Ready;
	};
	return Advance(InGraph, InReadCell, ReadBoundary, InWorkBudget, OutError, InCancel);
}

EVoxelHydrologyRegionStep FVoxelHydrologyLocalRunoffBuild::Advance(const FVoxelHydrologyRegionGraph& InGraph,
	FVoxelHydrologyRegionGraphBuild::FReadCell InReadCell, FReadBoundary InReadBoundary,
	int32 InWorkBudget, FString& OutError, const TAtomic<bool>* InCancel)
{
	OutError.Reset();
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return EVoxelHydrologyRegionStep::Canceled;
	}
	if (Stage == EStage::None || Stage == EStage::Taken || InGraph.Fingerprint != Result.GraphFingerprint ||
		InGraph.Generation != Result.Generation || InGraph.InputFingerprint != InputFingerprint ||
		InGraph.Minimum != Result.Minimum || InGraph.Width != Result.Width || InGraph.Height != Result.Height || InGraph.CellSize != Result.CellSize ||
		InGraph.ForestNeighbors.Num() != CellCount)
	{
		OutError = TEXT("HydrologyComponentConflict: local runoff frozen region changed");
		return Failure(OutError);
	}
	if (Stage == EStage::Ready) return EVoxelHydrologyRegionStep::Ready;
	static const FIntPoint Offsets[] = {FIntPoint(-1, 0), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(0, 1)};
	while (InWorkBudget-- > 0)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return EVoxelHydrologyRegionStep::Canceled;
		}
		if (Stage == EStage::Allocate)
		{
			const int32 BoundaryCount = CellCount - FMath::Max(0, Result.Width - 2) * FMath::Max(0, Result.Height - 2);
			if (!VoxelGenerationAllocation::RetainArray(Result.Cells, CellCount, OutError) ||
				!VoxelGenerationAllocation::RetainArray(States, CellCount, OutError) ||
				!VoxelGenerationAllocation::RetainArray(Queue, CellCount, OutError) ||
				!VoxelGenerationAllocation::RetainArray(Boundaries, BoundaryCount, OutError)) return Failure(OutError);
			Stage = EStage::Cells;
		}
		else if (Stage == EStage::Cells)
		{
			if (Cursor == CellCount)
			{
				if (CapturedInputFingerprint != InputFingerprint)
				{
					OutError = TEXT("HydrologyComponentConflict: local inputs differ from published macro forest");
					return Failure(OutError);
				}
				Cursor = 0;
				Stage = EStage::Boundary;
				continue;
			}
			FVoxelHydrologyLocalCell Cell;
			const auto Read = InReadCell(Position(Cursor), Cell.Sample, OutError);
			if (Read == EVoxelHydrologyClosureRead::Pending) return EVoxelHydrologyRegionStep::NeedInput;
			if (Read != EVoxelHydrologyClosureRead::Ready) return Failure(OutError);
			if (!Cell.Sample.IsValid())
			{
				OutError = TEXT("Invalid hydrology local ground, rainfall or receiver");
				return Failure(OutError);
			}
			Cell.Accumulation = Cell.Sample.IsReceiver() ? 0 : Cell.Sample.RainWeight;
			Result.LocalRainWeight += Cell.Accumulation;
			if (Cell.Sample.IsReceiver()) Cell.Potential = {Cell.Sample.WaterZ, 0};
			Result.Cells.Add(Cell);
			States.AddDefaulted();
			CapturedInputFingerprint = Cell.Sample.AppendFingerprint(CapturedInputFingerprint);
			++Cursor;
		}
		else if (Stage == EStage::Boundary)
		{
			if (Cursor == CellCount) { Cursor = 0; Stage = EStage::SpillSeeds; continue; }
			auto& Cell = Result.Cells[Cursor];
			if (!IsBoundary(Cursor)) { ++Cursor; continue; }
			FBoundary Boundary;
			Boundary.Index = Cursor;
			uint64 IncomingRain = 0;
			const auto Read = InReadBoundary(Position(Cursor), Boundary.Control, IncomingRain, OutError);
			if (Read == EVoxelHydrologyClosureRead::Pending) return EVoxelHydrologyRegionStep::NeedInput;
			if (Read != EVoxelHydrologyClosureRead::Ready) return Failure(OutError);
			const auto& Control = Boundary.Control;
			const int64 ParentDistance = FMath::Abs(int64(Control.ParentCell.X) - Control.Cell.X) +
				FMath::Abs(int64(Control.ParentCell.Y) - Control.Cell.Y);
			const int64 ParentX = int64(Control.ParentCell.X) * Result.CellSize;
			const int64 ParentY = int64(Control.ParentCell.Y) * Result.CellSize;
			const bool bPhysicalParent = ParentX + Result.CellSize > VoxelGeneration::WorldCellMin &&
				ParentY + Result.CellSize > VoxelGeneration::WorldCellMin &&
				ParentX < VoxelGeneration::WorldCellMaxExclusive && ParentY < VoxelGeneration::WorldCellMaxExclusive;
			if (Control.Cell != Position(Cursor) || Control.Potential.SpillZ == MAX_int32 ||
				Control.Potential.SpillZ < Cell.Sample.GroundZ || Control.Potential.PlateauDistance < 0 ||
				Control.Potential.PlateauDistance == MAX_int64 ||
				(Cell.Sample.IsReceiver() ? (Control.bHasParent || Control.Potential != Cell.Potential) :
					(!Control.bHasParent || ParentDistance != 1 || !bPhysicalParent)))
			{
				OutError = TEXT("HydrologyComponentConflict: unresolved or invalid local boundary control");
				return Failure(OutError);
			}
			if (MAX_uint64 - Result.IncomingRainWeight < IncomingRain || MAX_uint64 - Cell.Accumulation < IncomingRain)
			{
				OutError = TEXT("Oversized: local external rainfall");
				return Failure(OutError);
			}
			States[Cursor].Control = Boundaries.Add(Boundary);
			Cell.Potential = Control.Potential;
			Cell.Accumulation += IncomingRain;
			Result.IncomingRainWeight += IncomingRain;
			++Cursor;
		}
		else if (Stage == EStage::SpillSeeds)
		{
			if (Cursor == CellCount) { Cursor = 0; Stage = EStage::Spill; continue; }
			const auto& Cell = Result.Cells[Cursor];
			if ((Cell.Sample.IsReceiver() || States[Cursor].Control != INDEX_NONE) &&
				!Push({Cursor, Cell.Potential.SpillZ, 0}, OutError)) return Failure(OutError);
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
					Stage = bPlateaus ? EStage::Parents : EStage::Anchors;
					continue;
				}
				ActiveEvent = Pop();
				const auto& Cell = Result.Cells[ActiveEvent.Index];
				if (Cell.Potential.SpillZ != ActiveEvent.Plane ||
					(bPlateaus && Cell.Potential.PlateauDistance != ActiveEvent.Distance)) continue;
				Current = ActiveEvent.Index;
				Face = 0;
			}
			if (Face == 4) { Current = INDEX_NONE; continue; }
			if (InGraph.ForestNeighbors[Current] & (1 << Face))
			{
				const int32 Neighbor = IndexOf(Position(Current) + Offsets[Face]);
				check(Neighbor != INDEX_NONE);
				auto& Other = Result.Cells[Neighbor];
				if (!Other.Sample.IsReceiver())
				{
					if (!bPlateaus)
					{
						const int32 Plane = FMath::Max(ActiveEvent.Plane, Other.Sample.GroundZ);
						if (Plane < Other.Potential.SpillZ)
						{
							if (!Push({Neighbor, Plane, 0}, OutError)) return Failure(OutError);
							Other.Potential.SpillZ = Plane;
						}
					}
					else if (Other.Potential.SpillZ == ActiveEvent.Plane)
					{
						if (ActiveEvent.Distance >= MAX_int64 - 1)
						{
							OutError = TEXT("Oversized: local plateau distance");
							return Failure(OutError);
						}
						const int64 Distance = ActiveEvent.Distance + 1;
						if (Distance < Other.Potential.PlateauDistance)
						{
							if (!Push({Neighbor, ActiveEvent.Plane, Distance}, OutError)) return Failure(OutError);
							Other.Potential.PlateauDistance = Distance;
						}
					}
				}
			}
			++Face;
		}
		else if (Stage == EStage::Anchors || Stage == EStage::Parents)
		{
			const bool bParents = Stage == EStage::Parents;
			if (Cursor == CellCount) { Cursor = 0; Stage = bParents ? EStage::ValidateControls : EStage::Plateaus; continue; }
			auto& Cell = Result.Cells[Cursor];
			if (Cell.Potential.SpillZ == MAX_int32 || (bParents && Cell.Potential.PlateauDistance == MAX_int64))
			{
				OutError = TEXT("HydrologyComponentConflict: local dry component has no resolved outlet");
				return Failure(OutError);
			}
			if (States[Cursor].Control != INDEX_NONE)
			{
				const auto& Control = Boundaries[States[Cursor].Control].Control;
				if (Cell.Potential.SpillZ != Control.Potential.SpillZ)
				{
					OutError = TEXT("HydrologyComponentConflict: local forest lowered frozen boundary spill");
					return Failure(OutError);
				}
				if (bParents && Control.bHasParent && IndexOf(Control.ParentCell) == INDEX_NONE)
				{
					Cell.ParentCell = Control.ParentCell;
					Cell.bHasParent = true;
				}
			}
			const uint8 Mask = InGraph.ForestNeighbors[Cursor];
			for (int32 NeighborFace = 0; NeighborFace < 4 && !Cell.Sample.IsReceiver(); ++NeighborFace)
			{
				if (!(Mask & (1 << NeighborFace))) continue;
				const FIntPoint NeighborCell = Position(Cursor) + Offsets[NeighborFace];
				const auto& Other = Result.Cells[IndexOf(NeighborCell)];
				if (Other.Potential.SpillZ > Cell.Potential.SpillZ) continue;
				if (!bParents)
				{
					if (Other.Potential.SpillZ < Cell.Potential.SpillZ) Cell.Potential.PlateauDistance = 0;
				}
				else
				{
					const bool bDescending = Other.Potential.SpillZ < Cell.Potential.SpillZ;
					const bool bPlateauParent = Other.Potential.SpillZ == Cell.Potential.SpillZ &&
						Other.Potential.PlateauDistance < MAX_int64 && Other.Potential.PlateauDistance + 1 == Cell.Potential.PlateauDistance;
					if ((bDescending || bPlateauParent) && (!Cell.bHasParent || HydrologyLocalCellLess(NeighborCell, Cell.ParentCell)))
					{
						Cell.ParentCell = NeighborCell;
						Cell.bHasParent = true;
					}
				}
			}
			if (!bParents && Cell.Potential.PlateauDistance != MAX_int64 &&
				!Push({Cursor, Cell.Potential.SpillZ, Cell.Potential.PlateauDistance}, OutError)) return Failure(OutError);
			if (bParents && !Cell.Sample.IsReceiver() && !Cell.bHasParent)
			{
				OutError = TEXT("HydrologyComponentConflict: local dry runoff terminal");
				return Failure(OutError);
			}
			++Cursor;
		}
		else if (Stage == EStage::ValidateControls)
		{
			if (Cursor == Boundaries.Num()) { Cursor = 0; Stage = EStage::Children; continue; }
			const auto& Boundary = Boundaries[Cursor++];
			const auto& Cell = Result.Cells[Boundary.Index];
			if (Cell.Potential != Boundary.Control.Potential || Cell.bHasParent != Boundary.Control.bHasParent ||
				(Cell.bHasParent && Cell.ParentCell != Boundary.Control.ParentCell))
			{
				OutError = TEXT("HydrologyComponentConflict: local drainage disagrees with frozen boundary");
				return Failure(OutError);
			}
		}
		else if (Stage == EStage::Children)
		{
			if (Cursor == CellCount) { Cursor = 0; Stage = EStage::Queue; continue; }
			const auto& Cell = Result.Cells[Cursor++];
			const int32 Parent = Cell.bHasParent ? IndexOf(Cell.ParentCell) : INDEX_NONE;
			if (Parent != INDEX_NONE) ++States[Parent].Children;
		}
		else if (Stage == EStage::Queue)
		{
			if (Cursor == CellCount) { Cursor = 0; Stage = EStage::Accumulate; continue; }
			if (States[Cursor].Children == 0) Queue.Add(Cursor);
			++Cursor;
		}
		else if (Stage == EStage::Accumulate)
		{
			if (Cursor == Queue.Num())
			{
				if (Cursor != CellCount || MAX_uint64 - Result.LocalRainWeight < Result.IncomingRainWeight ||
					MAX_uint64 - Result.ReceivedRainWeight < Result.ExportedRainWeight ||
					Result.LocalRainWeight + Result.IncomingRainWeight != Result.ReceivedRainWeight + Result.ExportedRainWeight)
				{
					OutError = TEXT("HydrologyComponentConflict: local runoff cycle or rainfall conservation failure");
					return Failure(OutError);
				}
				Result.Fingerprint = VoxelGeneration::Mix(Result.GraphFingerprint ^ Result.LocalRainWeight ^ Result.IncomingRainWeight);
				Cursor = 0;
				Stage = EStage::Hash;
				continue;
			}
			const auto& Cell = Result.Cells[Queue[Cursor++]];
			const int32 Parent = Cell.bHasParent ? IndexOf(Cell.ParentCell) : INDEX_NONE;
			uint64& Destination = Parent != INDEX_NONE ? Result.Cells[Parent].Accumulation :
				(Cell.Sample.IsReceiver() ? Result.ReceivedRainWeight : Result.ExportedRainWeight);
			if (MAX_uint64 - Destination < Cell.Accumulation)
			{
				OutError = TEXT("Oversized: local rainfall accumulation");
				return Failure(OutError);
			}
			Destination += Cell.Accumulation;
			if (Parent != INDEX_NONE && --States[Parent].Children == 0) Queue.Add(Parent);
		}
		else if (Stage == EStage::Hash)
		{
			if (Cursor == CellCount)
			{
				if (!Result.Fingerprint) Result.Fingerprint = 1;
				FreeScratch();
				Stage = EStage::Ready;
				return EVoxelHydrologyRegionStep::Ready;
			}
			const auto& Cell = Result.Cells[Cursor++];
			Result.Fingerprint = VoxelGeneration::Mix(Result.Fingerprint ^ uint64(uint32(Cell.Potential.SpillZ)) ^ uint64(Cell.Potential.PlateauDistance));
			Result.Fingerprint = VoxelGeneration::Mix(Result.Fingerprint ^ ((uint64(uint32(Cell.ParentCell.X)) << 32) | uint32(Cell.ParentCell.Y)) ^ uint64(Cell.bHasParent));
			Result.Fingerprint = VoxelGeneration::Mix(Result.Fingerprint ^ Cell.Accumulation);
		}
	}
	return EVoxelHydrologyRegionStep::Yielded;
}

void FVoxelHydrologyLocalRunoffBuild::FreeScratch()
{
	VoxelGenerationAllocation::FreeRetainedArray(States);
	VoxelGenerationAllocation::FreeRetainedArray(Boundaries);
	VoxelGenerationAllocation::FreeRetainedArray(Frontier);
	VoxelGenerationAllocation::FreeRetainedArray(Queue);
}

const FVoxelHydrologyLocalRunoff& FVoxelHydrologyLocalRunoffBuild::GetResult() const
{
	check(Stage == EStage::Ready);
	return Result;
}

FVoxelHydrologyLocalRunoff FVoxelHydrologyLocalRunoffBuild::TakeResult()
{
	check(Stage == EStage::Ready);
	Stage = EStage::Taken;
	return MoveTemp(Result);
}

uint64 FVoxelHydrologyLocalRunoffBuild::GetAllocatedBytes() const
{
	return Result.GetAllocatedBytes() +
		VoxelGenerationAllocation::AllocatedBytes(States) + VoxelGenerationAllocation::AllocatedBytes(Boundaries) +
		VoxelGenerationAllocation::AllocatedBytes(Frontier) + VoxelGenerationAllocation::AllocatedBytes(Queue);
}
