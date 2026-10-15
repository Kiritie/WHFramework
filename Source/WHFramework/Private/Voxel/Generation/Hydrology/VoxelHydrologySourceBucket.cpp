#include "Voxel/Generation/Hydrology/VoxelHydrologySourceBucket.h"

#include "Voxel/Generation/VoxelGenerationMath.h"

FVoxelHydrologySourceBucketBuild::FVoxelHydrologySourceBucketBuild()
{
}

FVoxelHydrologySourceBucketBuild::~FVoxelHydrologySourceBucketBuild()
{
}

bool FVoxelHydrologySourceBucketBuild::Initialize(const FIntPoint& InCoordinate,
	const FVoxelGenerationSettings& InSettings, const uint64 InGeneration, FString& OutError)
{
	if (Stage != EStage::None || InSettings.HydrologyCellSize <= 0 || InSettings.RiverSourceSpacing <= 0 || !InGeneration)
	{
		OutError = TEXT("Invalid hydrology source bucket generation or settings");
		return false;
	}
	const int64 DomainMin = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMin, InSettings.HydrologyCellSize);
	const int64 DomainMax = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMaxExclusive - 1, InSettings.HydrologyCellSize);
	const int64 MinX = FMath::Max(DomainMin, int64(InCoordinate.X) * InSettings.RiverSourceSpacing);
	const int64 MinY = FMath::Max(DomainMin, int64(InCoordinate.Y) * InSettings.RiverSourceSpacing);
	const int64 MaxX = FMath::Min(DomainMax, (int64(InCoordinate.X) + 1) * InSettings.RiverSourceSpacing - 1);
	const int64 MaxY = FMath::Min(DomainMax, (int64(InCoordinate.Y) + 1) * InSettings.RiverSourceSpacing - 1);
	if (MinX > MaxX || MinY > MaxY)
	{
		OutError = TEXT("Hydrology source bucket is outside the physical world");
		return false;
	}
	if ((MaxX - MinX + 1) * (MaxY - MinY + 1) > MAX_int32)
	{
		OutError = TEXT("Oversized: hydrology source bucket candidate count");
		return false;
	}
	Minimum = FIntPoint(int32(MinX), int32(MinY));
	Maximum = FIntPoint(int32(MaxX), int32(MaxY));
	Cursor = Minimum;
	Seed = InSettings.Seed;
	CellSize = InSettings.HydrologyCellSize;
	Threshold = uint64(FMath::Max(1, InSettings.RiverSourceAccumulation));
	Result.Coordinate = InCoordinate;
	Result.Generation = InGeneration;
	Result.Fingerprint = VoxelGeneration::Mix(InGeneration ^ uint64(uint32(Seed)) ^ uint64(CellSize) ^ Threshold ^
		VoxelGeneration::Mix(uint64(InSettings.RiverSourceSpacing)) ^ ((uint64(uint32(InCoordinate.X)) << 32) | uint32(InCoordinate.Y)));
	Stage = EStage::Scan;
	OutError.Reset();
	return true;
}

EVoxelHydrologyRegionStep FVoxelHydrologySourceBucketBuild::Failure(FString& OutError) const
{
	if (OutError == TEXT("Canceled")) return EVoxelHydrologyRegionStep::Canceled;
	if (OutError.StartsWith(TEXT("CapacityBlocked"))) return EVoxelHydrologyRegionStep::CapacityBlocked;
	if (OutError.IsEmpty()) OutError = TEXT("HydrologyComponentConflict: invalid source bucket input");
	return EVoxelHydrologyRegionStep::Failed;
}

EVoxelHydrologyRegionStep FVoxelHydrologySourceBucketBuild::Advance(const uint64 InGeneration,
	FReadCell InReadCell, int32 InWorkBudget, FString& OutError, const TAtomic<bool>* InCancel)
{
	OutError.Reset();
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return EVoxelHydrologyRegionStep::Canceled;
	}
	if (Stage == EStage::None || Stage == EStage::Taken || InGeneration != Result.Generation)
	{
		OutError = TEXT("Invalid hydrology source bucket lifetime or generation");
		return Failure(OutError);
	}
	if (Stage == EStage::Ready) return EVoxelHydrologyRegionStep::Ready;
	while (InWorkBudget-- > 0)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return EVoxelHydrologyRegionStep::Canceled;
		}
		FVoxelHydrologySourceCell Cell;
		const auto Read = InReadCell(Cursor, Cell, OutError);
		if (Read == EVoxelHydrologyClosureRead::Pending) return EVoxelHydrologyRegionStep::NeedInput;
		if (Read != EVoxelHydrologyClosureRead::Ready) return Failure(OutError);
		const int64 ParentDistance = FMath::Abs(int64(Cell.ParentCell.X) - Cell.Cell.X) + FMath::Abs(int64(Cell.ParentCell.Y) - Cell.Cell.Y);
		const int64 DomainMin = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMin, CellSize);
		const int64 DomainMax = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMaxExclusive - 1, CellSize);
		const bool bPhysicalParent = Cell.ParentCell.X >= DomainMin && Cell.ParentCell.Y >= DomainMin &&
			Cell.ParentCell.X <= DomainMax && Cell.ParentCell.Y <= DomainMax;
		if (Cell.Cell != Cursor || Cell.MaxChildRainWeight > Cell.Accumulation || (Cell.bReceiver && Cell.bHasParent) ||
			(!Cell.bReceiver && (!Cell.bHasParent || ParentDistance != 1 || !bPhysicalParent)))
		{
			OutError = TEXT("Hydrology source bucket received incomplete cell facts");
			return Failure(OutError);
		}
		if (Cell.IsQualified(Threshold))
		{
			FIntPoint World;
			if (!VoxelGeneration::TryWorldPoint(FInt64Vector2(FMath::Max(int64(Cursor.X) * CellSize, VoxelGeneration::WorldCellMin),
				FMath::Max(int64(Cursor.Y) * CellSize, VoxelGeneration::WorldCellMin)), World))
			{
				OutError = TEXT("Invalid hydrology source candidate world position");
				return Failure(OutError);
			}
			const FVoxelStableId Id = VoxelGeneration::MakeStableId(Seed, FIntVector(World.X, World.Y, 0), 0x5249564552535243ull, 0);
			if (!Result.bHasWinner || Cell.Accumulation > Result.Winner.Accumulation ||
				(Cell.Accumulation == Result.Winner.Accumulation && Id < Result.WinnerId))
			{
				Result.Winner = Cell;
				Result.WinnerId = Id;
				Result.bHasWinner = true;
			}
		}
		Result.Fingerprint = VoxelGeneration::Mix(Result.Fingerprint ^ Cell.Accumulation ^ VoxelGeneration::Mix(Cell.MaxChildRainWeight));
		Result.Fingerprint = VoxelGeneration::Mix(Result.Fingerprint ^ ((uint64(uint32(Cell.ParentCell.X)) << 32) |
			uint32(Cell.ParentCell.Y)) ^ uint64(Cell.bHasParent) ^ (uint64(Cell.bReceiver) << 1));
		if (Cursor.X == Maximum.X)
		{
			Cursor.X = Minimum.X;
			if (Cursor.Y == Maximum.Y)
			{
				if (!Result.Fingerprint) Result.Fingerprint = 1;
				Stage = EStage::Ready;
				return EVoxelHydrologyRegionStep::Ready;
			}
			++Cursor.Y;
		}
		else ++Cursor.X;
	}
	return EVoxelHydrologyRegionStep::Yielded;
}

const FVoxelHydrologySourceBucket& FVoxelHydrologySourceBucketBuild::GetResult() const
{
	check(Stage == EStage::Ready);
	return Result;
}

FVoxelHydrologySourceBucket FVoxelHydrologySourceBucketBuild::TakeResult()
{
	check(Stage == EStage::Ready);
	Stage = EStage::Taken;
	return MoveTemp(Result);
}

uint64 FVoxelHydrologySourceBucketBuild::GetAllocatedBytes() const
{
	return 0;
}
