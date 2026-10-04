#pragma once

#include "CoreMinimal.h"

// 空间身份与采样身份分离；Level 始终相对于固定的 16-cell 网格。
enum class EVoxelTerrainRepresentation : uint8
{
	None = 0,
	Fine,
	VoxelProxy,
	Surface,
	Macro
};

struct WHFRAMEWORK_API FVoxelTerrainSpatialKey
{
	EVoxelTerrainRepresentation Representation = EVoxelTerrainRepresentation::None;
	FIntVector Coordinate = FIntVector::ZeroValue;
	uint8 Level = 0;

	bool operator==(const FVoxelTerrainSpatialKey& InOther) const = default;
	bool IsColumn() const;
	bool TryBounds(int32 InWorldMinZ, int32 InWorldMaxZ, FIntVector& OutMin, FIntVector& OutMax) const;
	int64 SideCells() const;
};

WHFRAMEWORK_API uint32 GetTypeHash(const FVoxelTerrainSpatialKey& InKey);

struct WHFRAMEWORK_API FVoxelTerrainGeometry
{
	FVoxelTerrainSpatialKey Key;
	int32 SampleStepCells = 1;
	int32 WorldMinZ = 0;
	int32 WorldMaxZ = 16;
	uint64 GeneratorIdentity = 0;
	uint64 MaterialRevision = 0;

	bool operator==(const FVoxelTerrainGeometry& InOther) const = default;
	bool Validate(FString& OutError) const;
};

FORCEINLINE uint32 GetTypeHash(const FVoxelTerrainGeometry& InGeometry)
{
	uint32 Hash = GetTypeHash(InGeometry.Key);
	Hash = HashCombineFast(Hash, GetTypeHash(InGeometry.SampleStepCells));
	Hash = HashCombineFast(Hash, GetTypeHash(InGeometry.WorldMinZ));
	Hash = HashCombineFast(Hash, GetTypeHash(InGeometry.WorldMaxZ));
	Hash = HashCombineFast(Hash, GetTypeHash(InGeometry.GeneratorIdentity));
	return HashCombineFast(Hash, GetTypeHash(InGeometry.MaterialRevision));
}

struct WHFRAMEWORK_API FVoxelTerrainBuildStamp
{
	uint64 WorldEpoch = 0;
	uint64 GeometryRevision = 0;
	uint64 ContentRevision = 0;
	uint64 DependencyRevision = 0;

	bool operator==(const FVoxelTerrainBuildStamp& InOther) const = default;
};

struct WHFRAMEWORK_API FVoxelTerrainBuildShard
{
	FIntVector LocalMinCells = FIntVector::ZeroValue;
	FIntVector CellCounts = FIntVector::ZeroValue;
};

struct WHFRAMEWORK_API FVoxelTerrainStageRange
{
	uint8 FirstLevel = 0;
	uint8 MaximumLevel = 0;
};

namespace VoxelStreamingGeometry
{
	inline constexpr int32 SectionSideCells = 16;
	inline constexpr uint8 MaximumSpatialLevel = 20;
	inline constexpr int32 MaximumInitialSideCells = 256;

	WHFRAMEWORK_API int64 FloorDivide(int64 InValue, int64 InDivisor);
	WHFRAMEWORK_API bool ValidateInitialSide(int32 InSide, uint8 InMaximumFineLevel, FString& OutError);
	WHFRAMEWORK_API bool TryLevel(int32 InSide, uint8& OutLevel, FString& OutError);
	WHFRAMEWORK_API bool DeriveNextStage(TConstArrayView<FVoxelTerrainGeometry> InActualBoundary,
		uint8 InMaximumLevel, FVoxelTerrainStageRange& OutRange, FString& OutError);
	WHFRAMEWORK_API bool ResolveSampleStep(double InNearestDistanceCells, double InVerticalFovDegrees,
		int32 InViewportHeight, double InTargetPixels, int32 InMaximumStepCells,
		int32& OutStepCells, bool& bOutAtResolutionFloor, FString& OutError);
	WHFRAMEWORK_API uint8 ResolveCoverageLevel(int32 InSampleStepCells, int32 InPreferredSamples,
		const FVoxelTerrainStageRange& InRange);
	WHFRAMEWORK_API int32 ResolveReplanStep(int32 InRadiusCells, double InFraction, int32 InMinimumSideCells);
	WHFRAMEWORK_API bool BuildShards(const FVoxelTerrainGeometry& InGeometry, int32 InMaximumSamplesPerAxis,
		int32 InMaximumShardCount, TArray<FVoxelTerrainBuildShard>& OutShards, FString& OutError);
}
