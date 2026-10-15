#include "Voxel/Generation/Hydrology/VoxelHydrologySourcePlan.h"

#include "Voxel/Generation/VoxelGenerationMath.h"

bool FVoxelHydrologySourcePlan::HasSourceInRange(const uint32 InFirst, const uint32 InEndExclusive) const
{
	int32 Lower = 0;
	int32 Upper = Sources.Num();
	while (Lower < Upper)
	{
		const int32 Middle = Lower + (Upper - Lower) / 2;
		if (Sources[Middle].Location < InFirst) Lower = Middle + 1;
		else Upper = Middle;
	}
	return Sources.IsValidIndex(Lower) && Sources[Lower].Location < InEndExclusive;
}

uint64 FVoxelHydrologySourcePlan::GetAllocatedBytes() const
{
	return VoxelGenerationAllocation::AllocatedBytes(Sources);
}

FVoxelHydrologySourcePlanBuild::FVoxelHydrologySourcePlanBuild()
{
}

FVoxelHydrologySourcePlanBuild::~FVoxelHydrologySourcePlanBuild()
{
}

bool FVoxelHydrologySourcePlanBuild::Initialize(const FVoxelHydrologyRegionGraph& InGraph,
	const FVoxelHydrologyNetworkRunoff& InRunoff, const FVoxelGenerationSettings& InSettings, FString& OutError)
{
	if (Stage != EStage::None || !InGraph.Fingerprint || !InRunoff.Fingerprint ||
		InRunoff.GraphFingerprint != InGraph.Fingerprint || InRunoff.Generation != InGraph.Generation ||
		InSettings.RiverSourceSpacing <= 0 || InSettings.HydrologyCellSize != InGraph.CellSize || InGraph.Width <= 0 || InGraph.Height <= 0 ||
		int64(InGraph.Width) * InGraph.Height != InGraph.CellLocations.Num())
	{
		OutError = TEXT("Invalid hydrology source plan inputs or lifetime");
		return false;
	}
	Result.Generation = InGraph.Generation;
	Result.GraphFingerprint = InGraph.Fingerprint;
	Result.RunoffFingerprint = InRunoff.Fingerprint;
	Threshold = uint64(FMath::Max(1, InSettings.RiverSourceAccumulation));
	Spacing = InSettings.RiverSourceSpacing;
	CellCount = InGraph.CellLocations.Num();
	Result.Fingerprint = VoxelGeneration::Mix(Result.Generation ^ Result.GraphFingerprint ^ Result.RunoffFingerprint ^ Threshold ^ uint64(Spacing));
	Stage = EStage::Scan;
	OutError.Reset();
	return true;
}

EVoxelHydrologyRegionStep FVoxelHydrologySourcePlanBuild::Failure(FString& OutError) const
{
	if (OutError == TEXT("Canceled")) return EVoxelHydrologyRegionStep::Canceled;
	if (OutError.StartsWith(TEXT("CapacityBlocked"))) return EVoxelHydrologyRegionStep::CapacityBlocked;
	if (OutError.IsEmpty()) OutError = TEXT("HydrologyComponentConflict: invalid source plan");
	return EVoxelHydrologyRegionStep::Failed;
}

void FVoxelHydrologySourcePlanBuild::Sift(int32 InRoot, const int32 InEnd)
{
	const auto Less = [](const FVoxelHydrologySelectedSource& InFirst, const FVoxelHydrologySelectedSource& InSecond)
	{
		return InFirst.Location < InSecond.Location || (InFirst.Location == InSecond.Location && InFirst.Id < InSecond.Id);
	};
	while (int64(InRoot) * 2 + 1 <= InEnd)
	{
		int32 Child = InRoot * 2 + 1;
		if (Child < InEnd && Less(Result.Sources[Child], Result.Sources[Child + 1])) ++Child;
		if (!Less(Result.Sources[InRoot], Result.Sources[Child])) break;
		Swap(Result.Sources[InRoot], Result.Sources[Child]);
		InRoot = Child;
	}
}

EVoxelHydrologyRegionStep FVoxelHydrologySourcePlanBuild::Advance(const FVoxelHydrologyRegionGraph& InGraph,
	const FVoxelHydrologyNetworkRunoff& InRunoff, FReadBucket InReadBucket, int32 InWorkBudget,
	FString& OutError, const TAtomic<bool>* InCancel)
{
	OutError.Reset();
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return EVoxelHydrologyRegionStep::Canceled;
	}
	if (Stage == EStage::None || Stage == EStage::Taken || InGraph.Generation != Result.Generation ||
		InGraph.Fingerprint != Result.GraphFingerprint || InRunoff.Generation != Result.Generation || InRunoff.Fingerprint != Result.RunoffFingerprint)
	{
		OutError = TEXT("Hydrology source plan frozen inputs changed");
		return Failure(OutError);
	}
	while (InWorkBudget-- > 0)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return EVoxelHydrologyRegionStep::Canceled;
		}
		if (Stage == EStage::Scan)
		{
			if (Cursor == CellCount)
			{
				SortRoot = Result.Sources.Num() / 2 - 1;
				SortEnd = Result.Sources.Num() - 1;
				Stage = EStage::Sort;
				continue;
			}
			const FIntPoint Cell = InGraph.Minimum + FIntPoint(Cursor % InGraph.Width, Cursor / InGraph.Width);
			FVoxelHydrologySourceCell Facts;
			if (!InRunoff.SampleSourceCell(InGraph, Cell, Facts, OutError)) return Failure(OutError);
			if (!Facts.IsQualified(Threshold)) { ++Cursor; continue; }
			const FIntPoint Bucket(VoxelGeneration::FloorDivide(Cell.X, Spacing), VoxelGeneration::FloorDivide(Cell.Y, Spacing));
			const FVoxelHydrologySourceBucket* Winner = nullptr;
			const auto Read = InReadBucket(Bucket, Winner, OutError);
			if (Read == EVoxelHydrologyClosureRead::Pending) return EVoxelHydrologyRegionStep::NeedInput;
			if (Read != EVoxelHydrologyClosureRead::Ready) return Failure(OutError);
			if (!Winner || Winner->Generation != Result.Generation || Winner->Coordinate != Bucket || !Winner->Fingerprint || !Winner->bHasWinner)
			{
				OutError = TEXT("Hydrology source plan requires complete matching global bucket winner");
				return Failure(OutError);
			}
			Result.Fingerprint = VoxelGeneration::Mix(Result.Fingerprint ^ Winner->Fingerprint);
			if (Winner->Winner.Cell != Cell) { ++Cursor; continue; }
			if (Winner->Winner.Accumulation != Facts.Accumulation || Winner->Winner.MaxChildRainWeight != Facts.MaxChildRainWeight)
			{
				OutError = TEXT("Hydrology global bucket winner disagrees with its source cell");
				return Failure(OutError);
			}
			Pending.Cell = Cell;
			Pending.Id = Winner->WinnerId;
			Pending.Accumulation = Facts.Accumulation;
			Attachment = Cell;
			WalkSteps = 0;
			Stage = EStage::Attach;
		}
		else if (Stage == EStage::Attach)
		{
			int32 Location = INDEX_NONE;
			const auto Kind = InGraph.FindCellLocation(Attachment, Location);
			if (Kind == EVoxelHydrologyRegionCellKind::Pruned)
			{
				if (++WalkSteps > CellCount)
				{
					OutError = TEXT("Hydrology source attachment has a cycle");
					return Failure(OutError);
				}
				const uint32 Parent = InGraph.PrunedCells[Location].Parent;
				Attachment = InGraph.Minimum + FIntPoint(Parent % InGraph.Width, Parent / InGraph.Width);
				continue;
			}
			if (Kind != EVoxelHydrologyRegionCellKind::Node && Kind != EVoxelHydrologyRegionCellKind::Profile)
			{
				OutError = TEXT("Hydrology source attachment left its canonical dry forest");
				return Failure(OutError);
			}
			Pending.Location = (Kind == EVoxelHydrologyRegionCellKind::Profile ? 1u << 30 : 0) | uint32(Location);
			if (Result.Sources.Num() == Result.Sources.Max())
			{
				const int32 Capacity = int32(FMath::Min<int64>(MAX_int32, FMath::Max<int64>(4, int64(Result.Sources.Num()) * 2)));
				if (Result.Sources.Num() == MAX_int32 || !VoxelGenerationAllocation::RetainArray(Result.Sources, Capacity, OutError)) return Failure(OutError);
			}
			Result.Sources.Add(Pending);
			++Cursor;
			Stage = EStage::Scan;
		}
		else if (Stage == EStage::Sort)
		{
			if (SortRoot >= 0) Sift(SortRoot--, SortEnd);
			else if (SortEnd > 0) { Swap(Result.Sources[0], Result.Sources[SortEnd]); Sift(0, --SortEnd); }
			else { Cursor = 0; Stage = EStage::Hash; }
		}
		else if (Stage == EStage::Hash)
		{
			if (Cursor < Result.Sources.Num())
			{
				const auto& Source = Result.Sources[Cursor++];
				Result.Fingerprint = VoxelGeneration::Mix(Result.Fingerprint ^ Source.Id.High ^ Source.Id.Low ^ Source.Accumulation ^ Source.Location);
				continue;
			}
			if (!Result.Fingerprint) Result.Fingerprint = 1;
			Stage = EStage::Ready;
			return EVoxelHydrologyRegionStep::Ready;
		}
		else if (Stage == EStage::Ready) return EVoxelHydrologyRegionStep::Ready;
	}
	return Stage == EStage::Ready ? EVoxelHydrologyRegionStep::Ready : EVoxelHydrologyRegionStep::Yielded;
}

const FVoxelHydrologySourcePlan& FVoxelHydrologySourcePlanBuild::GetResult() const
{
	check(Stage == EStage::Ready);
	return Result;
}

FVoxelHydrologySourcePlan FVoxelHydrologySourcePlanBuild::TakeResult()
{
	check(Stage == EStage::Ready);
	Stage = EStage::Taken;
	return MoveTemp(Result);
}

uint64 FVoxelHydrologySourcePlanBuild::GetAllocatedBytes() const
{
	return Result.GetAllocatedBytes();
}
