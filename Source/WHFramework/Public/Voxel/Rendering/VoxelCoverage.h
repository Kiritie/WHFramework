#pragma once

#include "CoreMinimal.h"

struct WHFRAMEWORK_API FVoxelCoverageRect
{
	FIntPoint Min =
		FIntPoint::ZeroValue;

	FIntPoint Max =
		FIntPoint::ZeroValue;

	bool IsValid() const
	{
		return
			Min.X < Max.X &&
			Min.Y < Max.Y;
	}

	bool ContainsCell(
		const FIntPoint& InPoint) const
	{
		return
			InPoint.X >= Min.X &&
			InPoint.Y >= Min.Y &&
			InPoint.X < Max.X &&
			InPoint.Y < Max.Y;
	}

	bool Intersects(
		const FVoxelCoverageRect& InOther) const
	{
		return
			Min.X < InOther.Max.X &&
			Max.X > InOther.Min.X &&
			Min.Y < InOther.Max.Y &&
			Max.Y > InOther.Min.Y;
	}
};

namespace VoxelCoverage
{
	/**
	 * 判断多个 half-open rect 的并集是否完整覆盖 Target。
	 *
	 * 只用于 LOD handoff；rect 数量是当前 ready representation 数，
	 * 不进入每 voxel 热路径。
	 */
	WHFRAMEWORK_API bool IsFullyCovered(
		const FVoxelCoverageRect& InTarget,
		TConstArrayView<FVoxelCoverageRect> InCoverage);
}
