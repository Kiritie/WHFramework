#include "Voxel/Rendering/VoxelStreamingGeometry.h"

uint32 GetTypeHash(const FVoxelTerrainSpatialKey& InKey)
{
	uint32 Hash = static_cast<uint8>(InKey.Representation);
	Hash = HashCombineFast(Hash, GetTypeHash(InKey.Coordinate.X));
	Hash = HashCombineFast(Hash, GetTypeHash(InKey.Coordinate.Y));
	Hash = HashCombineFast(Hash, GetTypeHash(InKey.Coordinate.Z));
	return HashCombineFast(Hash, InKey.Level);
}

bool FVoxelTerrainSpatialKey::IsColumn() const
{
	return Representation == EVoxelTerrainRepresentation::Surface ||
		Representation == EVoxelTerrainRepresentation::Macro;
}

int64 FVoxelTerrainSpatialKey::SideCells() const
{
	if (Level > VoxelStreamingGeometry::MaximumSpatialLevel)
	{
		return 0;
	}
	return int64(VoxelStreamingGeometry::SectionSideCells) << Level;
}

bool FVoxelTerrainSpatialKey::TryBounds(const int32 InWorldMinZ, const int32 InWorldMaxZ,
	FIntVector& OutMin, FIntVector& OutMax) const
{
	const int64 Side = SideCells();
	if (Side == 0 || Representation == EVoxelTerrainRepresentation::None ||
		static_cast<uint8>(Representation) > static_cast<uint8>(EVoxelTerrainRepresentation::Macro) ||
		(IsColumn() && (Coordinate.Z != 0 || InWorldMinZ >= InWorldMaxZ)))
	{
		return false;
	}
	FIntVector Min;
	FIntVector Max;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const int64 Begin = Axis == 2 && IsColumn() ? InWorldMinZ : int64(Coordinate[Axis]) * Side;
		const int64 End = Axis == 2 && IsColumn() ? InWorldMaxZ : Begin + Side;
		if (Begin < MIN_int32 || End > MAX_int32 || Begin >= End)
		{
			return false;
		}
		Min[Axis] = static_cast<int32>(Begin);
		Max[Axis] = static_cast<int32>(End);
	}
	OutMin = Min;
	OutMax = Max;
	return true;
}

bool FVoxelTerrainGeometry::Validate(FString& OutError) const
{
	FIntVector Min;
	FIntVector Max;
	if (!Key.TryBounds(WorldMinZ, WorldMaxZ, Min, Max) || SampleStepCells <= 0 ||
		!FMath::IsPowerOfTwo(SampleStepCells) || Key.SideCells() % SampleStepCells != 0 ||
		(Key.Representation == EVoxelTerrainRepresentation::Fine && SampleStepCells != 1))
	{
		OutError = TEXT("Invalid terrain geometry: representation, bounds, alignment or sampling");
		return false;
	}
	OutError.Reset();
	return true;
}

int64 VoxelStreamingGeometry::FloorDivide(const int64 InValue, const int64 InDivisor)
{
	check(InDivisor > 0);
	const int64 Quotient = InValue / InDivisor;
	return Quotient - (InValue % InDivisor < 0 ? 1 : 0);
}

bool VoxelStreamingGeometry::TryLevel(const int32 InSide, uint8& OutLevel, FString& OutError)
{
	if (InSide < SectionSideCells || InSide % SectionSideCells != 0 ||
		!FMath::IsPowerOfTwo(InSide / SectionSideCells))
	{
		OutError = FString::Printf(TEXT("Chunk side %d must be 16 * 2^N"), InSide);
		return false;
	}
	const uint32 Level = FMath::FloorLog2(static_cast<uint32>(InSide / SectionSideCells));
	if (Level > MaximumSpatialLevel)
	{
		OutError = TEXT("Chunk side exceeds the supported coordinate range");
		return false;
	}
	OutLevel = static_cast<uint8>(Level);
	OutError.Reset();
	return true;
}

bool VoxelStreamingGeometry::ValidateInitialSide(const int32 InSide,
	const uint8 InMaximumFineLevel, FString& OutError)
{
	uint8 Level = 0;
	if (!TryLevel(InSide, Level, OutError))
	{
		return false;
	}
	if (InSide > MaximumInitialSideCells || InMaximumFineLevel > 4 || Level > InMaximumFineLevel)
	{
		OutError = FString::Printf(TEXT("Initial side %d conflicts with the Fine limit %u; supported initial range is 16..256"),
			InSide, InMaximumFineLevel);
		return false;
	}
	return true;
}

bool VoxelStreamingGeometry::DeriveNextStage(TConstArrayView<FVoxelTerrainGeometry> InActualBoundary,
	const uint8 InMaximumLevel, FVoxelTerrainStageRange& OutRange, FString& OutError)
{
	if (InActualBoundary.IsEmpty() || InMaximumLevel > MaximumSpatialLevel)
	{
		OutError = TEXT("An actual predecessor frontier is required for stage derivation");
		return false;
	}
	uint8 BoundaryMaximum = 0;
	for (const FVoxelTerrainGeometry& Geometry : InActualBoundary)
	{
		if (!Geometry.Validate(OutError))
		{
			return false;
		}
		BoundaryMaximum = FMath::Max(BoundaryMaximum, Geometry.Key.Level);
	}
	if (BoundaryMaximum >= InMaximumLevel)
	{
		OutError = FString::Printf(TEXT("Required next stage level %u exceeds maximum %u"),
			uint32(BoundaryMaximum) + 1, InMaximumLevel);
		return false;
	}
	OutRange = {static_cast<uint8>(BoundaryMaximum + 1), InMaximumLevel};
	OutError.Reset();
	return true;
}

bool VoxelStreamingGeometry::ResolveSampleStep(const double InNearestDistanceCells,
	const double InVerticalFovDegrees, const int32 InViewportHeight, const double InTargetPixels,
	const int32 InMaximumStepCells, int32& OutStepCells, bool& bOutAtResolutionFloor, FString& OutError)
{
	if (!FMath::IsFinite(InNearestDistanceCells) || InNearestDistanceCells < 0.0 ||
		!FMath::IsFinite(InVerticalFovDegrees) || InVerticalFovDegrees <= 0.0 || InVerticalFovDegrees >= 180.0 ||
		InViewportHeight <= 0 || !FMath::IsFinite(InTargetPixels) || InTargetPixels <= 0.0 ||
		InMaximumStepCells <= 0 || !FMath::IsPowerOfTwo(InMaximumStepCells))
	{
		OutError = TEXT("Invalid projection or sample-step parameters");
		return false;
	}
	const double Projection = InViewportHeight /
		(2.0 * FMath::Tan(FMath::DegreesToRadians(InVerticalFovDegrees * 0.5)));
	const double Distance = FMath::Max(InNearestDistanceCells, 1.0);
	const double AllowedStep = InTargetPixels * Distance / Projection;
	int32 Step = 1;
	while (Step <= InMaximumStepCells / 2 && double(Step) * 2.0 <= AllowedStep)
	{
		Step *= 2;
	}
	OutStepCells = Step;
	bOutAtResolutionFloor = AllowedStep < 1.0;
	OutError.Reset();
	return true;
}

uint8 VoxelStreamingGeometry::ResolveCoverageLevel(const int32 InSampleStepCells,
	const int32 InPreferredSamples, const FVoxelTerrainStageRange& InRange)
{
	check(InSampleStepCells > 0 && InPreferredSamples > 0);
	check(InRange.FirstLevel <= InRange.MaximumLevel && InRange.MaximumLevel <= MaximumSpatialLevel);
	const int64 DesiredSide = int64(InSampleStepCells) * InPreferredSamples;
	uint8 Level = InRange.FirstLevel;
	while (Level < InRange.MaximumLevel && (int64(SectionSideCells) << (Level + 1)) <= DesiredSide)
	{
		++Level;
	}
	return Level;
}

int32 VoxelStreamingGeometry::ResolveReplanStep(const int32 InRadiusCells,
	const double InFraction, const int32 InMinimumSideCells)
{
	check(InRadiusCells >= 0 && FMath::IsFinite(InFraction) && InFraction >= 0.0 && InFraction <= 1.0);
	check(InMinimumSideCells > 0 && FMath::IsPowerOfTwo(InMinimumSideCells));
	if (InRadiusCells == 0)
	{
		return 1;
	}
	const int32 Ceiling = 1 << FMath::FloorLog2(static_cast<uint32>(InRadiusCells));
	const double Target = FMath::Min<double>(Ceiling,
		FMath::Max<double>(FMath::Min(InMinimumSideCells, Ceiling), InRadiusCells * InFraction));
	int32 Lower = 1;
	while (Lower <= Ceiling / 2 && Lower * 2.0 <= Target)
	{
		Lower *= 2;
	}
	const int32 Upper = Lower <= Ceiling / 2 ? Lower * 2 : Lower;
	return Target - Lower <= Upper - Target ? Lower : Upper;
}

bool VoxelStreamingGeometry::BuildShards(const FVoxelTerrainGeometry& InGeometry,
	const int32 InMaximumSamplesPerAxis, const int32 InMaximumShardCount,
	TArray<FVoxelTerrainBuildShard>& OutShards, FString& OutError)
{
	if (!InGeometry.Validate(OutError))
	{
		return false;
	}
	if (InMaximumSamplesPerAxis <= 0 || InMaximumShardCount <= 0)
	{
		OutError = TEXT("Invalid build shard limits");
		return false;
	}
	const int64 CellsPerAxis = InGeometry.Key.SideCells() / InGeometry.SampleStepCells;
	const int64 Parts = (CellsPerAxis + InMaximumSamplesPerAxis - 1) / InMaximumSamplesPerAxis;
	const bool bColumn = InGeometry.Key.IsColumn();
	if (Parts > InMaximumShardCount || Parts * Parts > InMaximumShardCount ||
		(!bColumn && Parts * Parts > InMaximumShardCount / Parts))
	{
		OutError = TEXT("Build would exceed the shard metadata limit; demand is retained and reported as blocked");
		return false;
	}
	TArray<FVoxelTerrainBuildShard> Candidate;
	Candidate.Reserve(static_cast<int32>(Parts * Parts * (bColumn ? 1 : Parts)));
	const int32 SideSamples = static_cast<int32>(CellsPerAxis);
	for (int32 Z = 0; Z < (bColumn ? 1 : SideSamples); Z += InMaximumSamplesPerAxis)
	{
		for (int32 Y = 0; Y < SideSamples; Y += InMaximumSamplesPerAxis)
		{
			for (int32 X = 0; X < SideSamples; X += InMaximumSamplesPerAxis)
			{
				FVoxelTerrainBuildShard& Shard = Candidate.AddDefaulted_GetRef();
				Shard.LocalMinCells = FIntVector(X, Y, bColumn ? 0 : Z) * InGeometry.SampleStepCells;
				Shard.CellCounts = FIntVector(FMath::Min(InMaximumSamplesPerAxis, SideSamples - X),
					FMath::Min(InMaximumSamplesPerAxis, SideSamples - Y),
					bColumn ? 0 : FMath::Min(InMaximumSamplesPerAxis, SideSamples - Z));
			}
		}
	}
	OutShards = MoveTemp(Candidate);
	OutError.Reset();
	return true;
}
