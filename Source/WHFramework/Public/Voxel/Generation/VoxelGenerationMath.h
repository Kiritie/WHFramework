#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"

namespace VoxelGeneration
{
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

	WHFRAMEWORK_API bool IsInsideCapsule(
		const FIntVector& InPosition,
		const FIntVector& InStart,
		const FIntVector& InEnd,
		int32 InRadius);

	WHFRAMEWORK_API uint64 HashBytes(
		TConstArrayView<uint8> InBytes);
}
