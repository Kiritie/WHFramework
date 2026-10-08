#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"

namespace VoxelGeneration
{
	// int32 半开 Bounds 的统一 cell 域；MAX_int32 仅作排他上界。
	inline constexpr int64 WorldCellMin = MIN_int32;
	inline constexpr int64 WorldCellMaxExclusive = MAX_int32;
	WHFRAMEWORK_API int64 FloorDivideWide(int64 InValue, int32 InDivisor);
	WHFRAMEWORK_API bool IsWorldCell(int64 InCoordinate);
	WHFRAMEWORK_API bool TryWorldPoint(const FInt64Vector2& InCoordinate, FIntPoint& OutPoint);
	WHFRAMEWORK_API bool TryWorldCell(const FInt64Vector& InCoordinate, FIntVector& OutCell);
	WHFRAMEWORK_API bool TryRoundedWorldCell(const FVector& InCoordinate, FIntVector& OutCell);
	WHFRAMEWORK_API bool ClipWorldBounds(const FInt64Vector& InMin, const FInt64Vector& InMax,
		FVoxelGenerationBounds& OutBounds);

	WHFRAMEWORK_API int32 FloorDivide(int32 InValue, int32 InDivisor);
	WHFRAMEWORK_API int32 CeilDividePositive(int32 InValue, int32 InDivisor);

	WHFRAMEWORK_API uint64 Mix(uint64 InValue);

	WHFRAMEWORK_API uint64 MakeSeed(
		int32 InWorldSeed,
		const FIntVector& InAnchor,
		uint64 InSalt);

	WHFRAMEWORK_API FVoxelStableId MakeStableId(
		int32 InWorldSeed,
		const FIntVector& InAnchor,
		uint64 InKind,
		uint64 InOrdinal = 0);

	WHFRAMEWORK_API int32 RandomRange(
		uint64 InSeed,
		int32 InMin,
		int32 InMax);

	WHFRAMEWORK_API int32 Noise2D(
		int32 InWorldSeed,
		int32 InX,
		int32 InY,
		int32 InPeriod,
		uint64 InSalt);

	WHFRAMEWORK_API int32 Noise3D(
		int32 InWorldSeed,
		const FIntVector& InPosition,
		int32 InPeriod,
		uint64 InSalt);

	WHFRAMEWORK_API int64 Dot(
		const FIntVector& InA,
		const FIntVector& InB);

	WHFRAMEWORK_API bool IsInsideEllipsoid(
		const FIntVector& InPosition,
		const FIntVector& InCenter,
		const FIntVector& InRadius);
	WHFRAMEWORK_API bool IsInsideEllipsoid(
		const FIntVector& InPosition,
		const FInt64Vector& InCenter,
		const FIntVector& InRadius);

	WHFRAMEWORK_API bool IsInsideCapsule(
		const FIntVector& InPosition,
		const FIntVector& InStart,
		const FIntVector& InEnd,
		int32 InRadius);

	WHFRAMEWORK_API uint64 HashBytes(
		TConstArrayView<uint8> InBytes);
}
